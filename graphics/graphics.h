#ifndef GRAPHICS_H
#define GRAPHICS_H

// VGA Mode 0x13 - 320x200, 256 colors
#define VGA_WIDTH   320
#define VGA_HEIGHT  200
#define VGA_MEMORY  0xA0000
#define TASKBAR_H   14

// Font size (8x8)
#define FONT_W  8
#define FONT_H  8

// Color indices (mode 13h default palette)
#define COL_BLACK    0
#define COL_BLUE     1
#define COL_GREEN    2
#define COL_CYAN     3
#define COL_RED      4
#define COL_MAGENTA  5
#define COL_BROWN    6
#define COL_GREY     7
#define COL_DGREY    8
#define COL_LBLUE    9
#define COL_LGREEN   10
#define COL_LCYAN    11
#define COL_LRED     12
#define COL_LMAGENTA 13
#define COL_YELLOW   14
#define COL_WHITE    15

// Core graphics
void gfx_init();
void gfx_clear(unsigned char color);
void gfx_pixel(int x, int y, unsigned char color);
void gfx_rect(int x, int y, int w, int h, unsigned char color);
void gfx_rect_fill(int x, int y, int w, int h, unsigned char color);
void gfx_char(int x, int y, char c, unsigned char fg, unsigned char bg);
void gfx_str(int x, int y, const char* s, unsigned char fg, unsigned char bg);

// Terminal
void term_init();
void term_putc(char c, unsigned char fg);
void term_puts(const char* s, unsigned char fg);
void term_putln(const char* s, unsigned char fg);
void term_clear();
void term_prompt();

extern int term_cx;
extern int term_cy;

// UI
void ui_draw_desktop();
void ui_draw_taskbar();
void ui_draw_window(int x, int y, int w, int h, const char* title);
void ui_draw_boot_screen();

#endif
