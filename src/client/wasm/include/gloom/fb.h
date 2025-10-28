#ifndef FB_H_
#define FB_H_

#include <gloom/types.h>
#include <gloom/macros.h>

#define FB_WIDTH  640
#define FB_HEIGHT 480

extern u32 _fb[FB_WIDTH * FB_HEIGHT];

#define FB_SIZE   sizeof(_fb)
#define FB_LEN    ARRLEN(_fb)

static inline
void fb_set_pixel(u32 x, u32 y, u32 c) {
  _fb[x + y * FB_WIDTH] = c;
}

static inline
void fb_set_pixel_index(u32 i, u32 c) {
  _fb[i] = c;
}

static inline
u32 fb_get_pixel(u32 x, u32 y) {
  return _fb[x + y * FB_WIDTH];
}

static inline
u32 fb_get_pixel_index(u32 i) {
  return _fb[i];
}

#endif
