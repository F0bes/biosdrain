#pragma once

#include <draw.h>
#include <dma.h>

void fontengine_init(void);
qword_t *fontengine_print_string(qword_t *q, const char *str, int x, int y, int z);

extern const u32 FE_WIDTH;
extern const u32 FE_HEIGHT;
