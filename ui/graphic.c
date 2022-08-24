#include "menu.h"

#include <kernel.h>
#include <graph.h>
#include <draw.h>
#include <gif_tags.h>
#include <gif_registers.h>
#include <gs_gp.h>
#include <gs_psm.h>
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

// main.c frame stuff

extern framebuffer_t g_fb;
extern zbuffer_t g_zb;

// Texture stuff
extern unsigned int size_bongo_tex_1;
extern unsigned char bongo_tex_1[];

extern unsigned int size_bongo_tex_2;
extern unsigned char bongo_tex_2[];

extern unsigned int size_biosdrain_tex;
extern unsigned char biosdrain_tex[];

u32 g_biosdrain_texaddress = 0;
u32 g_bongo1_texaddress = 0;
u32 g_bongo2_texaddress = 0;
void graphic_init_load_textures(void)
{
	g_biosdrain_texaddress = graph_vram_allocate(256, 256, GS_PSM_32, GRAPH_ALIGN_BLOCK);
	g_bongo1_texaddress = graph_vram_allocate(256, 256, GS_PSM_32, GRAPH_ALIGN_BLOCK);
	g_bongo2_texaddress = graph_vram_allocate(256, 256, GS_PSM_32, GRAPH_ALIGN_BLOCK);

	qword_t *texBuffer = (qword_t *)aligned_alloc(64, sizeof(qword_t) * 270);

	qword_t *q = texBuffer;

	{ // Biosdrain logo
		q = draw_texture_transfer(q, &biosdrain_tex[0], 256, 256, 0, g_biosdrain_texaddress, 256);
		q = draw_texture_flush(q);

		dma_channel_send_chain(DMA_CHANNEL_GIF, texBuffer, q - texBuffer, 0, 0);
		dma_channel_wait(DMA_CHANNEL_GIF, 0);
	}
	{ // Bongo 1
		q = texBuffer;
		q = draw_texture_transfer(q, &bongo_tex_1[0], 256, 256, 0, g_bongo1_texaddress, 256);
		q = draw_texture_flush(q);

		dma_channel_send_chain(DMA_CHANNEL_GIF, texBuffer, q - texBuffer, 0, 0);
		dma_channel_wait(DMA_CHANNEL_GIF, 0);
	}
	{ // Bongo 2
		q = texBuffer;
		q = draw_texture_transfer(q, &bongo_tex_2[0], 256, 256, 0, g_bongo2_texaddress, 256);
		q = draw_texture_flush(q);
		dma_channel_send_chain(DMA_CHANNEL_GIF, texBuffer, q - texBuffer, 0, 0);
		dma_channel_wait(DMA_CHANNEL_GIF, 0);
	}
	{ // Set bilinear
		q = texBuffer;
		PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_DISABLE, 0, GIF_FLG_PACKED, 1),
			GIF_REG_AD);
		q++;
		q->dw[0] = GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0);
		q->dw[1] = GS_REG_TEX1;
		q++;
		dma_channel_send_normal(DMA_CHANNEL_GIF, texBuffer, q - texBuffer, 0, 0);
		dma_channel_wait(DMA_CHANNEL_GIF, 0);
	}
	free(texBuffer);
}

static qword_t* biosdrain_texture_rgbaq = NULL;
s32 graphic_vsync_sema;
void graphic_biosdrain_fadein()
{
	for (int a = 0; a < 255; a++)
	{
		WaitSema(graphic_vsync_sema);
		biosdrain_texture_rgbaq->dw[1] = (u64)((0xFF) | ((u64)a << 32));
	}
}

void graphic_init(void)
{
	graphic_init_load_textures();

	qword_t* gif_packet = (qword_t*)aligned_alloc(64, sizeof(qword_t) * 10);

	qword_t* q = gif_packet;
	// Set registers that only need to be set once and clear the screen
	{
		PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_ENABLE, GIF_PRIM_SPRITE, GIF_FLG_PACKED, 7),
			GIF_REG_AD | (GIF_REG_AD << 4) | (GIF_REG_AD << 8) | (GIF_REG_RGBAQ << 12) | (GIF_REG_XYZ2 << 16) | (GIF_REG_XYZ2 << 20) | (GIF_REG_AD << 24));
		q++;
		// ALPHA
		// Cs * As
		q->dw[0] = GS_SET_ALPHA(0, 2, 0, 2, 0);
		q->dw[1] = GS_REG_ALPHA;
		q++;
		// XYOFFSET
		q->dw[0] = GS_SET_XYOFFSET(0, 0);
		q->dw[1] = GS_REG_XYOFFSET;
		q++;
		// TEST (Set it to pass everything, I've had OSDSYS garbage be a high Z value
		// and it required setting test to always pass so we can clear it)
		q->dw[0] = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, ZTEST_METHOD_ALLPASS);
		q->dw[1] = GS_REG_TEST;
		q++;
		// RGBAQ
		q->dw[0] = (u64)((0) | ((u64)0x00 << 32));
		q->dw[1] = (u64)((0x00) | ((u64)0xFF << 32));
		q++;
		// XYZ2
		q->dw[0] = (u64)((((0 << 4)) | (((u64)(0 << 4)) << 32)));
		q->dw[1] = (u64)(0); // Write to 0, so our future draws pass the ztest
		q++;
		// XYZ2
		q->dw[0] = (u64)((((g_fb.width << 4)) | (((u64)(448 << 4)) << 32)));
		q->dw[1] = (u64)(0); // Write to 0, so our future draws pass the ztest
		q++;
		// TEST
		q->dw[0] = GS_SET_TEST(1, ATEST_METHOD_NOTEQUAL, 0, 0, 0, 0, 1, ZTEST_METHOD_GREATER_EQUAL);
		q->dw[1] = GS_REG_TEST;
		q++;
		dma_channel_send_normal(DMA_CHANNEL_GIF, gif_packet, q - gif_packet, 0, 0);
		dma_channel_wait(DMA_CHANNEL_GIF, 0);
	}
	free(gif_packet);

	u64 *VU1_DATA_PTR = (u64 *)0x1100C000;
	VU1_DATA_PTR[0] = GS_SET_TEX0(g_bongo1_texaddress / 64, 4, 0, 8, 8, 1, 1, 0, 0, 0, 0, 0);
	VU1_DATA_PTR[1] = GS_REG_TEX0;

	VU1_DATA_PTR[2] = GS_SET_TEX0(g_bongo2_texaddress / 64, 4, 0, 8, 8, 1, 1, 0, 0, 0, 0, 0);
	VU1_DATA_PTR[3] = GS_REG_TEX0;

	// addr(2)
	qword_t *VU1_QW = (qword_t *)&VU1_DATA_PTR[4];

	PACK_GIFTAG(VU1_QW, GIF_SET_TAG(1, 0, GIF_PRE_ENABLE, GIF_PRIM_SPRITE, GIF_FLG_PACKED, 5),
		(GIF_REG_AD) | (GIF_REG_RGBAQ << 4) | (GIF_REG_XYZ2 << 8) | (GIF_REG_XYZ2 << 12) | (GIF_REG_AD << 16));
	VU1_QW++;
	{ // Sprite clear
		// TEST (Make the zbuffer pass everything, so we can clear this area)
		VU1_QW->dw[0] = GS_SET_TEST(1, ATEST_METHOD_NOTEQUAL, 0, 0, 0, 0, 1, ZTEST_METHOD_ALLPASS);
		VU1_QW->dw[1] = GS_REG_TEST;
		VU1_QW++;
		// RGBAQ
		VU1_QW->dw[0] = (u64)((0) | ((u64)0x00 << 32));
		VU1_QW->dw[1] = (u64)((0x00) | ((u64)0xFF << 32));
		VU1_QW++;
		// XYZ2
		VU1_QW->dw[0] = (u64)((((364 << 4)) | (((u64)(310 << 4)) << 32)));
		VU1_QW->dw[1] = (u64)(0); // Write to 0, so our future draws pass the ztest
		VU1_QW++;
		// XYZ2
		VU1_QW->dw[0] = (u64)((((664 << 4)) | (((u64)(440 << 4)) << 32)));
		VU1_QW->dw[1] = (u64)(0); // Write to 0, so our future draws pass the ztest
		VU1_QW++;
		// TEST (Restore Z testing)
		VU1_QW->dw[0] = GS_SET_TEST(1, ATEST_METHOD_NOTEQUAL, 0, 0, 0, 0, 1, ZTEST_METHOD_GREATER_EQUAL);
		VU1_QW->dw[1] = GS_REG_TEST;
		VU1_QW++;
	}
	// addr(8)
	PACK_GIFTAG(VU1_QW, GIF_SET_TAG(2, 1, GIF_PRE_ENABLE, GIF_SET_PRIM(GIF_PRIM_SPRITE, 0, 1, 0, 1, 0, 0, 0, 0), GIF_FLG_PACKED, 6),
		GIF_REG_AD | (GIF_REG_ST << 4) | (GIF_REG_RGBAQ << 8) | (GIF_REG_XYZ2 << 12) | (GIF_REG_ST << 16) | (GIF_REG_XYZ2 << 20));
	VU1_QW++;
	{ // Bongo cat
		// TEX0 (Gets replaced by the VU depending on the current texture frame)
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
		VU1_QW->dw[0] = (u64)((((464 << 4)) | (((u64)(315 << 4)) << 32)));
		VU1_QW->dw[1] = (u64)(2);
		VU1_QW++;
		// ST
		VU1_QW->dw[0] = GIF_SET_ST(0x3f800000, 0x3f800000);
		VU1_QW->dw[1] = 0x3f800000;
		VU1_QW++;
		// XYZ2
		VU1_QW->dw[0] = (u64)(((574 << 4)) | (((u64)(375 << 4)) << 32));
		VU1_QW->dw[1] = (u64)(2);
		VU1_QW++;
	}
	// addr(15)
	{ // Bios drain
		// TEX0
		VU1_QW->dw[0] = GS_SET_TEX0(g_biosdrain_texaddress / 64, 4, 0, 8, 8, 0, 1, 0, 0, 0, 0, 0);
		VU1_QW->dw[1] = GS_REG_TEX0;
		VU1_QW++;
		// UV
		VU1_QW->dw[0] = GS_SET_ST(0, 0);
		VU1_QW->dw[1] = 0x3f800000;
		VU1_QW++;
		biosdrain_texture_rgbaq = VU1_QW; // Store this pointer, so we can do a fade in by changing the alpha here
		// RGBAQ
		VU1_QW->dw[0] = (u64)((0xFF) | ((u64)0xFF << 32));
		VU1_QW->dw[1] = (u64)((0xFF) | ((u64)0 << 32));
		VU1_QW++;
		// XYZ2
		VU1_QW->dw[0] = (u64)((((364 << 4)) | (((u64)(340 << 4)) << 32)));
		VU1_QW->dw[1] = (u64)(1);
		VU1_QW++;
		// ST
		VU1_QW->dw[0] = GIF_SET_ST(0x3f800000, 0x3e9eb852);
		VU1_QW->dw[1] = 0x3f800000;
		VU1_QW++;
		// XYZ2
		VU1_QW->dw[0] = (u64)((((620 << 4)) | (((u64)(420 << 4)) << 32)));
		VU1_QW->dw[1] = (u64)(1);
		VU1_QW++;
	}
	// addr(21)

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
