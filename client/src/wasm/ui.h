#ifndef __DRAW_H__
#define __DRAW_H__

#include <client.h>
#include <font.h>

enum button_state {
  BUTTON_IDLE = 0,
  BUTTON_HOVER,
};

struct button {
  vec2u tl, br;
  enum button_state state;
  const char* text;
  void (*on_click)(void);
};

extern vec2u __cur;
extern u32 __fg_color, __bg_color;

static inline void set_cursor_x(u32 x) { __cur.x = x % FB_WIDTH; }
static inline void set_cursor_y(u32 y) { __cur.y = y % FB_HEIGHT; }

static inline u32 get_cursor_x(void) { return __cur.x; }
static inline u32 get_cursor_y(void) { return __cur.y; }

static inline void set_colors(u32 fg, u32 bg) {
  __fg_color = fg;
  __bg_color = bg;
}

void draw_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
void draw_button(u32 x, u32 y, struct button* b);

void write_text_with_color(u32 scale, u32 color, const char* text);

static inline void write_text(u32 scale, const char* text) {
  write_text_with_color(scale, __fg_color, text);
}

static inline void clear_screen_with_color(u32 color) {
  u32 i;
  for (i = 0; i < FB_LEN; ++i)
    fb[i] = color;
}

static inline void clear_screen(void) { clear_screen_with_color(__bg_color); }

static inline void ui_on_mouse_move(u32 x, u32 y, struct button* buttons, u32 n) {
  u32 i;
  struct button* b;

  for (i = 0; i < n; ++i) {
    b = &buttons[i];
    b->state =
      (x >= b->tl.x && y >= b->tl.y && x <= b->br.x && y <= b->br.y)
      ? BUTTON_HOVER : BUTTON_IDLE;
  }
}

static inline void ui_on_mouse_click(struct button* buttons, u32 n) {
  u32 i;
  for (i = 0; i < n; ++i) {
    if (buttons[i].state == BUTTON_HOVER && buttons[i].on_click)
      buttons[i].on_click();
  }
}

static inline void ui_on_enter(struct button* buttons, u32 n) {
  u32 i;
  for (i = 0; i < n; ++i)
    buttons[i].state = BUTTON_IDLE;
}

#endif
