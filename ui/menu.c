#include "config.h"
#include "menu.h"
#include "fontengine.h"
#include "fontqueue.h"

#include <kernel.h>
#include <sio.h>
#include <debug.h>
#include <graph.h>
#include <draw.h>
#include <dma.h>
#include <gs_gp.h>
#include <gs_psm.h>

#include <stdio.h>
#include <stdlib.h> // aligned_alloc
#include <stdarg.h>

framebuffer_t g_fb;
zbuffer_t g_zb;

static void intc_vsync_handler(s32 alarm_id, u16 time, void* common)
{
	graphic_draw_fast();
	fontqueue_kick();
	ExitHandler();
}

static void gs_init(void)
{
	// Reset GIF
	(*(volatile u_int*)0x10003000) = 1;

	g_fb.width = 640;
	g_fb.height = 448;
	g_fb.psm = GS_PSM_24;
	g_fb.address = graph_vram_allocate(g_fb.width, g_fb.height, g_fb.psm, GRAPH_ALIGN_PAGE);
	printf("framebuffer at %08X\n", g_fb.address);
	g_zb.enable = 1;
	g_zb.method = ZTEST_METHOD_GREATER;
	g_zb.mask = 0;
	g_zb.zsm = GS_PSMZ_24;
	g_zb.address = graph_vram_allocate(g_fb.width, g_fb.height, g_zb.zsm, GRAPH_ALIGN_PAGE);
	printf("zbuffer at %08X\n", g_zb.address);
	graph_initialize(g_fb.address, g_fb.width, g_fb.height, g_fb.psm, 0, 0);

	qword_t setup_packet[30] __attribute__((aligned(64)));
	qword_t* q = setup_packet;

	q = draw_setup_environment(q, 0, &g_fb, &g_zb);
	dma_channel_send_normal(DMA_CHANNEL_GIF, setup_packet, q - setup_packet, 0, 0);
	dma_channel_wait(DMA_CHANNEL_GIF, 0);

	q = setup_packet;
	q = draw_clear(q, 0, 0, 0, g_fb.width, g_fb.height, 0xFF, 0, 0);
	dma_channel_send_normal(DMA_CHANNEL_GIF, setup_packet, q - setup_packet, 0, 0);
	dma_channel_wait(DMA_CHANNEL_GIF, 0);
}

void menu_init(void)
{
	gs_init();
	fontqueue_init(4096);
	graph_wait_vsync();

	DIntr();
	AddIntcHandler(INTC_VBLANK_S, (void*)intc_vsync_handler, 0);
	EnableIntc(INTC_VBLANK_S);
	EIntr();
}

// TODO: proper newline support?
void menu_status(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char str[256];
	vsnprintf(str, sizeof(str), fmt, args);
	sio_puts(str);

	fontqueue_add_simple(str);

	vprintf(str, args);
}
