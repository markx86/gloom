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
    struct {
      union {
        b8 ticked; // checkbox
        f32 value; // slider
      };
      u32 pad;
    };
  };
};

extern u32 __fg_color, __bg_color;

#define TITLE_HEIGHT     (FONT_HEIGHT * 2 * 3)
#define TITLE_WIDTH(s)   ((strlen(s) + 4) * FONT_WIDTH * 2)

#define STRING_HEIGHT       (FONT_HEIGHT)
#define STRING_WIDTH(s)     (strlen(s) * FONT_WIDTH)
#define STRING_WIDTH_IMM(s) ((sizeof(s)-1) * FONT_WIDTH)

#define SLIDER_WIDTH     128

static inline void ui_set_colors(u32 fg, u32 bg) {
  __fg_color = fg;
  __bg_color = bg;
}

void draw_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
void draw_component(u32 x, u32 y, struct component* b);
void draw_title(u32 x, u32 y, const char* text);
void draw_string(u32 x, u32 y, const char* text);
void draw_string_with_color(u32 x, u32 y, const char* text, u32 color);

static inline void clear_screen_with_color(u32 color) {
  u32 i;
  for (i = 0; i < FB_LEN; ++i)
    set_pixel_index(i, color);
}

static inline void clear_screen(void) { clear_screen_with_color(__bg_color); }

static inline b8 component_is_mouse_over(u32 x, u32 y, struct component* c) {
  return (x >= c->tl.x && y >= c->tl.y && x <= c->br.x && y <= c->br.y);
}

static inline void component_on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy, struct component* comps, u32 n) {
  u32 i;
  b8 interacting;
  struct component* c;

  UNUSED(dy);

  // if the user is interacting with a ui element, do not process
  // other hover events
  interacting = false;
  for (i = 0; i < n; ++i) {
    if (comps[i].state == UICOMP_PRESSED) {
      interacting = true;
      break;
    }
  }

  for (i = 0; i < n; ++i) {
    c = &comps[i];
    if (c->state != UICOMP_PRESSED)
      c->state = !interacting && component_is_mouse_over(x, y, c) ? UICOMP_HOVER : UICOMP_IDLE;
    else if (c->type == UICOMP_SLIDER) {
      c->value += (f32)dx / (SLIDER_WIDTH << 1);
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
