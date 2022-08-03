#pragma once

#include <tamtypes.h>

void fontqueue_init(u32 qw);
void fontqueue_add_full(char* str, int* x, int* y, int z);
void fontqueue_add_simple(char* str);
void fontqueue_reset_simple(void);
void fontqueue_kick(void);
