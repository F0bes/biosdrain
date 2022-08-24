#pragma once

void menu_init(void);
void menu_status(const char* fmt, ...);

void graphic_init(void);
void graphic_draw_fast(void);
void graphic_biosdrain_fadein(void);

extern int graphic_vsync_sema;
