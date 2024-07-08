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

s32 vsync_thread_id;
static s32 intc_vsync_handler(s32 cause)
{
	iSignalSema(graphic_vsync_sema);
	ExitHandler();
	return 0;
}

void vsync_thread_func(void* unused)
{
	while (1)
	{
		graphic_draw_fast();
		fontqueue_kick();
		WaitSema(graphic_vsync_sema);
	}
}
static void gs_init(void)
{
	// Reset GIF
	(*(volatile u32*)0x10003000) = 1;

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
	fontqueue_init(8192);
	graph_wait_vsync();


	ee_thread_t vsync_thread;
	vsync_thread.func = vsync_thread_func;
	vsync_thread.stack = aligned_alloc(16, 0x100);
	vsync_thread.stack_size = 0x100;
	vsync_thread.initial_priority = 010;
	vsync_thread.gp_reg = &_gp;
	vsync_thread.attr = 0;
	vsync_thread.option = 0;

	ee_sema_t vsync_sema;

	vsync_sema.attr = 0;
	vsync_sema.option = 0;
	vsync_sema.max_count = 1;
	vsync_sema.init_count = 0;
	vsync_sema.wait_threads = 2;

	graphic_vsync_sema = CreateSema(&vsync_sema);

	vsync_thread_id = CreateThread(&vsync_thread);
	StartThread(vsync_thread_id, NULL);


	DIntr();
	DisableIntc(INTC_VBLANK_S);
	AddIntcHandler(INTC_VBLANK_S, (void*)intc_vsync_handler, 0);
	EnableIntc(INTC_VBLANK_S);
	EIntr();
}

// TODO: proper newline support?
void menu_status(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char str[1024];
	vsnprintf(str, sizeof(str), fmt, args);
	sio_puts(str);

	fontqueue_add_simple(str);

	vprintf(str, args);
}
