#ifndef UI_H_
#define UI_H_

#include <gloom/fb.h>
/* Add include to color.h, since it's often used with ui.h */
#include <gloom/color.h>

/* Define strlen(..) to avoid including libc.h */
extern u32 strlen(const char*);

#include "font.h"

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
    /* Button */
    struct {
      void (*on_click)(void);
      /* Padding to make this struct the same size as the one below */
      char _unused[(sizeof(u32) + sizeof(f32)) - sizeof(void*)];
    };
    struct {
      union {
        b8 ticked; /* Checkbox */
        f32 value; /* Slider */
      };
      u32 pad;
    };
  };
};

extern u32 _g_fg_color, _g_bg_color;

#define TITLE_HEIGHT       (FONT_HEIGHT * 2 * 3)
#define TITLE_WIDTH(s)     ((strlen(s) + 4) * FONT_WIDTH * 2)
#define TITLE_WIDTH_IMM(s) ((sizeof(s)-1 + 4) * FONT_WIDTH * 2)

#define STRING_HEIGHT       (FONT_HEIGHT)
#define STRING_WIDTH(s)     (strlen(s) * FONT_WIDTH)
#define STRING_WIDTH_IMM(s) ((sizeof(s)-1) * FONT_WIDTH)

#define SLIDER_WIDTH 128

static inline
void ui_set_colors(u32 fg, u32 bg) {
  _g_fg_color = fg;
  _g_bg_color = bg;
}

void ui_draw_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
void ui_draw_component(u32 x, u32 y, struct component* b);
void ui_draw_title(u32 x, u32 y, const char* text);
void ui_draw_string(u32 x, u32 y, const char* text);
void ui_draw_string_with_color(u32 x, u32 y, const char* text, u32 color);

static inline
void ui_clear_screen_with_color(u32 color) {
  u32 i;
  for (i = 0; i < FB_LEN; ++i)
    fb_set_pixel_index(i, color);
}

static inline
void ui_clear_screen(void) {
  ui_clear_screen_with_color(_g_bg_color);
}

void ui_on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy,
                       struct component* comps, u32 n);
void ui_on_mouse_down(u32 x, u32 y, struct component* comps, u32 n);
void ui_on_mouse_up(u32 x, u32 y, struct component* comps, u32 n);
void ui_on_enter(struct component* comps, u32 n);

#endif
