#include <gloom/fb.h>

u32 _fb[FB_WIDTH * FB_HEIGHT];

void* gloom_framebuffer(void) {
  return _fb;
}

u32 gloom_framebuffer_width(void) {
  return FB_WIDTH;
}

u32 gloom_framebuffer_height(void) {
  return FB_HEIGHT;
}

u32 gloom_framebuffer_size(void) {
  return FB_SIZE;
}
