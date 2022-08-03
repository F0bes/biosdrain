#include "fontqueue.h"
#include "fontengine.h"

#include <tamtypes.h>
#include <stdlib.h>
#include <dma.h>
#include <stdio.h>
#include <sio.h>
#include <assert.h>
#include <string.h>

static qword_t *font_queue;
static u32 cur_qw_cnt = 0;
static u32 max_qw_cnt = 0;
void fontqueue_init(u32 qw)
{
	font_queue = (qword_t *)aligned_alloc(64, sizeof(qword_t) * qw);
	max_qw_cnt = qw;
}

static u32 simple_yaxis = 0;
void fontqueue_add_simple(char *str)
{
	// fontengine takes 3 + (4 * strlen(str)) QW
	if ((cur_qw_cnt + (3 + (4 * strlen(str)))) > max_qw_cnt)
	{
		printf("OUT OF FONTQUEUE SPACE\n");
		return;
	}
	cur_qw_cnt = (fontengine_print_string(&font_queue[cur_qw_cnt], str, 0, simple_yaxis, 10) - font_queue);

	simple_yaxis += FE_HEIGHT;
}

void fontqueue_reset_simple(void)
{
	simple_yaxis = 0;
}

void fontqueue_add_full(char *str, float x, float y, float z)
{
	// fontengine takes 3 + (4 * strlen(str)) QW
	if ((cur_qw_cnt + (3 + (4 * strlen(str)))) > max_qw_cnt)
	{
		printf("OUT OF FONTQUEUE SPACE\n");
		sio_puts("OUT OF FONTQUEUE SPACE\n");
		return;
	}
	cur_qw_cnt = (fontengine_print_string(&font_queue[cur_qw_cnt], str, x, y, z) - font_queue);
}

void fontqueue_clear(void)
{
	cur_qw_cnt = 0;
}

void fontqueue_kick(void)
{
	if (!cur_qw_cnt)
		return;

	dma_channel_send_normal(DMA_CHANNEL_GIF, font_queue, cur_qw_cnt, 0, 0);
	dma_channel_wait(DMA_CHANNEL_GIF, 0);
}
