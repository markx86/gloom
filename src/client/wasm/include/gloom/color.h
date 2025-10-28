#ifndef COLOR_H_
#define COLOR_H_

#include <gloom/types.h>

enum color {
  COLOR_BLACK,
  COLOR_GRAY,
  COLOR_LIGHTGRAY,
  COLOR_WHITE,
  COLOR_DARKRED,
  COLOR_RED,
  COLOR_DARKGREEN,
  COLOR_GREEN,
  COLOR_DARKYELLOW,
  COLOR_YELLOW,
  COLOR_DARKBLUE,
  COLOR_BLUE,
  COLOR_DARKMAGENTA,
  COLOR_MAGENTA,
  COLOR_DARKCYAN,
  COLOR_CYAN,
  COLOR_MAX
};

extern const u32 _color_palette[COLOR_MAX];
extern u32 _color_alpha_mask;

static inline
void color_set_alpha(u8 a) {
  _color_alpha_mask = ((u32)a) << 24;
}

static inline
u32 color_get_alpha_mask(void) {
  return _color_alpha_mask;
}

static inline
u32 color_get(u8 index) {
  return _color_alpha_mask | _color_palette[index];
}

static inline
u32 color_get_solid(u8 index) {
  return 0xFF000000 | _color_palette[index];
}

#define COLOR(x)        color_get(COLOR_##x)
#define SOLID_COLOR(x)  color_get_solid(COLOR_##x)

#endif
