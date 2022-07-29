#include "menu.h"

#include <kernel.h>
#include <graph.h>
#include <draw.h>
#include <gif_tags.h>
#include <gif_registers.h>
#include <gs_gp.h>
#include <dma.h>
#include <stdio.h>
#include <stdlib.h> // aligned_alloc

extern u64 graphic_vu_start __attribute__((section(".vudata")));
extern u64 graphic_vu_end __attribute__((section(".vudata")));

// VIF stuff
#define VIFMPG(size, pos) ((0x4A000000) | (size << 16) | pos)
#define VIFFLUSHE 0x10000000
#define VIFFLUSH 0x11000000
#define VIFFLUSHA 0x13000000
#define VIFMSCAL(execaddr) (0x14000000 | execaddr)
#define VIFMSCNT 0x17000000
#define VIFFMSCALF(execaddr) (0x15000000 | execaddr)
#define VIF1CHCR (*(volatile u32 *)0x10009000)
#define VIF1MADR (*(volatile u32 *)0x10009010)
#define VIF1QWC (*(volatile u32 *)0x10009020)

// Texture stuff
extern unsigned int size_bongo_tex_1;
extern unsigned char bongo_tex_1[];

extern unsigned int size_bongo_tex_2;
extern unsigned char bongo_tex_2[];

u32 TEX1ADDR = 0xB0000;
u32 TEX2ADDR = 0xC0000;
void graphic_init_load_textures(void)
{

	qword_t *texBuffer = (qword_t *)aligned_alloc(64, sizeof(qword_t) * 270);
	qword_t *q = texBuffer;
	q = draw_texture_transfer(q, &bongo_tex_1[0], 256, 256, 0, TEX1ADDR, 256);
	q = draw_texture_flush(q);

	dma_channel_send_chain(DMA_CHANNEL_GIF, texBuffer, q - texBuffer, 0, 0);
	dma_channel_wait(DMA_CHANNEL_GIF, 0);

	q = texBuffer;
	q = draw_texture_transfer(q, &bongo_tex_2[0], 256, 256, 0, TEX2ADDR, 256);
	q = draw_texture_flush(q);

	dma_channel_send_chain(DMA_CHANNEL_GIF, texBuffer, q - texBuffer, 0, 0);
	dma_channel_wait(DMA_CHANNEL_GIF, 0);

	free(texBuffer);
}

void graphic_init(void)
{
	graphic_init_load_textures();
	u64 *VU1_DATA_PTR = (u64 *)0x1100C000;
	VU1_DATA_PTR[0] = GS_SET_TEX0(TEX1ADDR / 64, 4, 0, 8, 8, 0, 1, 0, 0, 0, 0, 0); // Set texture #1 here
	VU1_DATA_PTR[1] = GS_REG_TEX0;												   // Set texture #2 here

	VU1_DATA_PTR[2] = GS_SET_TEX0(TEX2ADDR / 64, 4, 0, 8, 8, 0, 1, 0, 0, 0, 0, 0); // Set texture #1 here
	VU1_DATA_PTR[3] = GS_REG_TEX0;												   // Set texture #2 here

	// addr(2)
	qword_t *VU1_QW = (qword_t *)&VU1_DATA_PTR[4];

	PACK_GIFTAG(VU1_QW, GIF_SET_TAG(1, 1, GIF_PRE_ENABLE, GIF_SET_PRIM(GIF_PRIM_SPRITE, 0, 1, 0, 0, 0, 0, 0, 0), GIF_FLG_PACKED, 6),
				GIF_REG_AD | (GIF_REG_ST << 4) | (GIF_REG_RGBAQ << 8) | (GIF_REG_XYZ2 << 12) | (GIF_REG_ST << 16) | (GIF_REG_XYZ2 << 20));
	VU1_QW++;
	// TEX0 (To be copied by VU1)
	VU1_QW->dw[0] = 0xb000b;
	VU1_QW->dw[1] = 0;
	VU1_QW++;
	// ST
	VU1_QW->dw[0] = GIF_SET_ST(0, 0);
	VU1_QW->dw[1] = 0x3f800000;
	VU1_QW++;
	// RGBAQ
	VU1_QW->dw[0] = (u64)((0) | ((u64)0xFC << 32));
	VU1_QW->dw[1] = (u64)((0xFE) | ((u64)0x1 << 32));
	VU1_QW++;
	// XYZ2
	// DUE TO LACK OF Z BUFFER CURRENTLY, WE CANNOT HAVE THE CAT
	// BONGOING THE LOGO. IT CAUSES Z FIGHTING LIKE ISSUES :(.
	// WHEN WE HAVE A PROPER UI, THIS CAN BE FIXED :).
	VU1_QW->dw[0] = (u64)((((400 << 4)) | (((u64)(45 << 4)) << 32)));
	VU1_QW->dw[1] = (u64)(0);
	VU1_QW++;
	// ST
	VU1_QW->dw[0] = GIF_SET_ST(0x3f800000, 0x3f800000);
	VU1_QW->dw[1] = 0x3f800000;
	VU1_QW++;
	// XYZ2
	VU1_QW->dw[0] = (u64)(((510 << 4)) | (((u64)(105 << 4)) << 32));
	VU1_QW->dw[1] = (u64)(0);
	VU1_QW++;

	// addr(9)
	FlushCache(0);

	u64 microInstructioncnt = (&graphic_vu_end - &graphic_vu_start);
	if (microInstructioncnt > 255)
	{
		printf("WARNING, THE MICRO PROGRAM IS > THAN 255 INSTRUCTIONS!\n");
	}

	u32 vif_packet[2000] __attribute__((aligned(64)));
	u32 vif_index = 0;

	// Wait for any possible micro programs
	vif_packet[vif_index++] = VIFFLUSHE;

	// Upload the micro program to address 0
	vif_packet[vif_index++] = VIFMPG(microInstructioncnt, 0);

	// Embed the micro program into the VIF packet
	for (u32 instr = 0; instr < microInstructioncnt; instr++)
	{
		vif_packet[vif_index++] = (&graphic_vu_start)[instr];
		vif_packet[vif_index++] = (&graphic_vu_start)[instr] >> 32;
	}

	// Execute the micro program at address 0
	vif_packet[vif_index++] = VIFMSCAL(0);

	while (vif_index % 4)				// Align the packet
		vif_packet[vif_index++] = 0x00; // VIFNOP

	VIF1MADR = (u32)vif_packet;
	VIF1QWC = vif_index / 4;

	FlushCache(0);

	VIF1CHCR = 0x101;

	while (VIF1CHCR & 0x100)
	{
	};
}

void graphic_draw_fast(void)
{
	// Reset the GIF every vsync? But it works so whatever...
	// Could probably remove once the dependency on debug draw is gone.
	// My suspicion is that scr_printf is causing the GIF to wait for PATH3?
	(*(volatile u_int *)0x10003000) = 1;

	asm volatile(
		"vu1_active_%=:\n"
		"bc2t vu1_active_%=\n"
		"nop\n"
		:);
	// continue the micro program
	asm volatile(
		"li $t0, 0x43A\n"
		"CTC2 $t0, $vi1\n"
		"VILWR.x $vi2, ($vi1)\n"
		"CFC2 $t0, $vi2\n"
		"CTC2 $t0, $31\n" ::
			: "$t0");
}
