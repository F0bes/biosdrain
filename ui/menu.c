#include "config.h"
#include "menu.h"

#include <kernel.h>
#include <debug.h>
#include <graph.h>
#include <draw.h>
#include <dma.h>
#include <gs_gp.h>

#include <stdio.h>
#include <stdlib.h> // aligned_alloc
#include <stdarg.h>

// Simple menu system using debug, just for now

extern unsigned int size_biosdrain_tex;
extern unsigned char biosdrain_tex[];

// Unfix this and allocate properly when
// a proper menu is developed.
static u32 g_logo_texaddress = 0x96000;

static qword_t g_draw_packet[10] __attribute__((aligned(64)));

void menu_load_logo()
{
	qword_t *texBuffer = (qword_t *)aligned_alloc(64, sizeof(qword_t) * 270);
	qword_t *q = texBuffer;

	q = draw_texture_transfer(q, &biosdrain_tex[0], 256, 256, 0, g_logo_texaddress, 256);
	q = draw_texture_flush(q);

	dma_channel_send_chain(DMA_CHANNEL_GIF, texBuffer, q - texBuffer, 0, 0);
	dma_channel_wait(DMA_CHANNEL_GIF, 0);

	// Reuse the texBuffer

	q = texBuffer;
	// Set registers that only need to be set once
	{
		PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_DISABLE, 0, GIF_FLG_PACKED, 3),
					GIF_REG_AD | (GIF_REG_AD << 4) | (GIF_REG_AD << 8));
		q++;
		// ALPHA
		q->dw[0] = GS_SET_ALPHA(0, 2, 0, 2, 0);
		q->dw[1] = GS_REG_ALPHA;
		q++;
		// XYOFFSET
		q->dw[0] = GS_SET_XYOFFSET(0, 0);
		q->dw[1] = GS_REG_XYOFFSET;
		q++;
		// TEST
		q->dw[0] = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);
		q->dw[1] = GS_REG_TEST;
		q++;

		dma_channel_send_normal(DMA_CHANNEL_GIF, texBuffer, q - texBuffer, 0, 0);
		dma_channel_wait(DMA_CHANNEL_GIF, 0);
	}
	free(texBuffer);

	// Craft our packet to draw the logo
	q = g_draw_packet;
	{
		PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_ENABLE, GS_SET_PRIM(GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0), GIF_FLG_PACKED, 6),
					(GIF_REG_AD) | (GIF_REG_RGBAQ << 4) | (GIF_REG_UV << 8) | (GIF_REG_XYZ2 << 12) | (GIF_REG_UV << 16) | (GIF_REG_XYZ2 << 20));
		q++;
		// TEX0
		q->dw[0] = GS_SET_TEX0(g_logo_texaddress / 64, 4, 0, 8, 8, 0, 1, 0, 0, 0, 0, 0);
		q->dw[1] = GS_REG_TEX0;
		q++;
		// RGBAQ
		q->dw[0] = (u64)((0xFF) | ((u64)0xFF << 32));
		q->dw[1] = (u64)((0xFF) | ((u64)0x00 << 32));
		q++;
		// UV
		q->dw[0] = GS_SET_ST(0, 0);
		q->dw[1] = (u64)(0);
		q++;
		// XYZ2
		q->dw[0] = (u64)((((300 << 4)) | (((u64)(100 << 4)) << 32)));
		q->dw[1] = (u64)(0);
		q++;
		// UV
		q->dw[0] = GS_SET_ST(256 << 4, 80 << 4);
		q->dw[1] = (u64)(0);
		q++;
		// XYZ2
		q->dw[0] = (u64)((((556 << 4)) | (((u64)(180 << 4)) << 32)));
		q->dw[1] = (u64)(0);
		q++;
	}
}

void menu_logo_draw()
{
	dma_channel_send_normal(DMA_CHANNEL_GIF, g_draw_packet, 8, 0, 0);
	dma_channel_wait(DMA_CHANNEL_GIF, 0);
}

void menu_logo_alpha(u16 alpha)
{
	g_draw_packet[2].dw[0] = (u64)((0xFF) | ((u64)0xFF << 32));
	g_draw_packet[2].dw[1] = (u64)((0xFF) | ((u64)alpha << 32));
	menu_logo_draw();
}

void menu_logo_fadein()
{
	menu_logo_alpha(0);
	for (int i = 0; i < 255; i++)
	{
		menu_logo_alpha(i);
		graph_wait_vsync();
		graph_wait_vsync();
	}
}

static const s32 graphic_vsync_max = 5;
static s32 graphic_vsync_current = 0;
static void intc_vsync_handler(s32 alarm_id, u16 time, void *common)
{
	if(graphic_vsync_current++ == graphic_vsync_max)
	{
		graphic_vsync_current = 0;
		graphic_draw_fast();
	}
	ExitHandler();
}

void menu_init(void)
{
	init_scr();
	scr_clear();

	menu_load_logo();

	graph_wait_vsync();

	scr_setCursor(0);
	scr_setXY(0, 1);

	scr_printf("BiosDrain Starting. rev %s\n", GIT_VERSION);

	DIntr();
	AddIntcHandler(INTC_VBLANK_S, (void*)intc_vsync_handler, 0);
	EnableIntc(INTC_VBLANK_S);
	EIntr();

	graphic_init();
}

void menu_status(const char *fmt, ...)
{
	// This is interesting
	// There is no scr_vprintf()
	// Allocate 128 byte the stack for the arguments?
	// Unsure what would be best here
	void *arg = __builtin_apply_args();
	__builtin_apply((void *)scr_printf, arg, 128);
	__builtin_apply((void *)printf, arg, 128);
	menu_logo_draw();
}
