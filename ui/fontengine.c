#include "fontengine.h"

#include <tamtypes.h>
#include <draw.h>
#include <graph.h>
#include <gs_psm.h>
#include <gs_gp.h>

#include <stdlib.h>
#include <string.h>

// Linked in
extern unsigned int size_font_tex;
extern unsigned char font_tex[];

extern unsigned int size_font_pallete_tex;
extern unsigned char font_pallete_tex[];

extern framebuffer_t g_fb;

const u32 FE_WIDTH = 18;
const u32 FE_HEIGHT = 14;

static u32 font_indexed_addr = 0;
static u32 font_pallete_addr = 0;

void fontengine_init(void)
{
	// Our framebuffer is CT24, the high 8 bits can be used for our 8H indexed texture
	font_indexed_addr = g_fb.address;
	font_pallete_addr = graph_vram_allocate(16, 16, GS_PSM_32, GRAPH_ALIGN_BLOCK);
	qword_t *gif_packet = (qword_t *)aligned_alloc(64, sizeof(qword_t) * 2048);
	qword_t *q = gif_packet;

	q = gif_packet;
	q = draw_texture_transfer(q, font_tex, 1024, 512, GS_PSM_8H, font_indexed_addr, 1024);

	dma_channel_send_chain(DMA_CHANNEL_GIF, gif_packet, (q - gif_packet), 0, 0);
	dma_channel_wait(DMA_CHANNEL_GIF, 0);

	q = gif_packet;

	q = draw_texture_transfer(q, font_pallete_tex, 16, 16, GS_PSM_32, font_pallete_addr, 64);
	q = draw_texture_flush(q);

	dma_channel_send_chain(DMA_CHANNEL_GIF, gif_packet, (q - gif_packet), 0, 0);
	dma_channel_wait(DMA_CHANNEL_GIF, 0);

	free(gif_packet);
}

qword_t *fontengine_print_string(qword_t *q, const char *str, int x, int y, int z)
{
	PACK_GIFTAG(q, GIF_SET_TAG(1, 0, GIF_PRE_DISABLE, 0, GIF_FLG_PACKED, 2), GIF_REG_AD | (GIF_REG_AD << 4));
	q++;
	// TEX0 (Point to the indexed fontmap and CLUT pallette)
	q->dw[0] = GS_SET_TEX0(font_indexed_addr / 64, 16, GS_PSM_8H, 10, 10, 1, 1, font_pallete_addr / 64, GS_PSM_32, 0, 0, 1);
	q->dw[1] = GS_REG_TEX0;
	q++;
	// TEX1 (Enable Bilinear)
	q->dw[0] = GS_SET_TEX1(1,0,1,1,0,0,0);
	q->dw[1] = GS_REG_TEX1;
	q++;

	size_t char_cnt = strlen(str);
	PACK_GIFTAG(q, GIF_SET_TAG(char_cnt, 1, GIF_PRE_ENABLE, GIF_SET_PRIM(GIF_PRIM_SPRITE, 0, 1, 0, 0, 0, 1, 0, 0), GIF_FLG_PACKED, 4),
				GIF_REG_UV | (GIF_REG_XYZ2 << 4) | (GIF_REG_UV << 8) | (GIF_REG_XYZ2 << 12));
	q++;

	for (int i = 0; i < char_cnt; i++)
	{
		u32 UVX = (str[i] % 16) * 64;
		u32 UVY = ((str[i] / 16) - 2) * 64;
		// UV (Offset by 0.5, might make it look better?)
		q->dw[0] = GS_SET_ST((UVX << 4) + 8, (UVY << 4) + 8);
		q->dw[1] = 0;
		q++;
		// XYZ2
		q->dw[0] = (u64)(((((i * (FE_WIDTH / 2)) + x)) << 4) | (((u64)(y << 4)) << 32));
		q->dw[1] = (u64)(z);
		q++;
		// UV
		q->dw[0] = GS_SET_ST(((UVX + 63) << 4) + 8, ((UVY + 64) << 4) + 8);
		q->dw[1] = 0;
		q++;
		// XYZ2
		q->dw[0] = (u64)(((((i * (FE_WIDTH / 2)) + x) + (FE_WIDTH)) << 4) | (((u64)((y + FE_HEIGHT) << 4)) << 32));
		q->dw[1] = (u64)(z);
		q++;
	}
	return q;
}
