#ifndef __DRAW_H__
#define __DRAW_H__

#include <client.h>
#include <font.h>

enum component_state {
  UICOMP_IDLE = 0,
  UICOMP_HOVER,
  UICOMP_PRESSED,
};

enum component_type {
  UICOMP_BUTTON,
  UICOMP_SLIDER,
  UICOMP_CHECKBOX
};

struct component {
  enum component_type type;
  enum component_state state;
  vec2u tl, br;
  const char* text;
  union {
    // button
    struct {
      void (*on_click)(void);
    };
    // slider
    struct {
      f32 value;
      u32 width;
    };
    // checkbox
    struct {
      b8 ticked;
      u32 pad;
    };
  };
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
void draw_component(u32 x, u32 y, struct component* b);

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

static inline b8 component_is_mouse_over(u32 x, u32 y, struct component* c) {
  return (x >= c->tl.x && y >= c->tl.y && x <= c->br.x && y <= c->br.y);
}

static inline void component_on_mouse_move(u32 x, u32 y, i32 dx, i32 dy, struct component* comps, u32 n) {
  u32 i;
  struct component* c;

  UNUSED(dy);

  for (i = 0; i < n; ++i) {
    c = &comps[i];
    if (c->state != UICOMP_PRESSED)
      c->state = component_is_mouse_over(x, y, c) ? UICOMP_HOVER : UICOMP_IDLE;
    else if (c->type == UICOMP_SLIDER) {
      c->value += (f32)dx / c->width;
      if (c->value > 1.0f)
        c->value = 1.0f;
      else if (c->value < 0.0f)
        c->value = 0.0f;
    }
  }
}

static inline void component_on_mouse_down(u32 x, u32 y, struct component* comps, u32 n) {
  u32 i;
  struct component* c;

  for (i = 0; i < n; ++i) {
    c = &comps[i];
    if (c->state == UICOMP_HOVER && component_is_mouse_over(x, y, c))
      c->state = UICOMP_PRESSED;
  }
}

static inline void component_on_mouse_up(u32 x, u32 y, struct component* comps, u32 n) {
  u32 i;
  struct component* c;

  for (i = 0; i < n; ++i) {
    c = &comps[i];
    if (c->state == UICOMP_PRESSED) {
      if (component_is_mouse_over(x, y, c)) {
        switch (c->type) {
          case UICOMP_BUTTON:
            if (c->on_click)
              c->on_click();
            break;
          case UICOMP_CHECKBOX:
            c->ticked = !c->ticked;
            break;
          case UICOMP_SLIDER:
            break;
        }
        c->state = UICOMP_HOVER;
      } else
        c->state = UICOMP_IDLE;
    }
  }
}

static inline void component_on_enter(struct component* comps, u32 n) {
  u32 i;
  for (i = 0; i < n; ++i)
    comps[i].state = UICOMP_IDLE;
}

#endif
