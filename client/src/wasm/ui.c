#define DEFINE_FONT
#include <ui.h>

vec2u __cur = {.x = 0, .y = 0};
u32 __fg_color = 0xFFFFFFFF, __bg_color = 0xFF000000;

static inline b8 can_draw(u32* x, u32* y, u32* w, u32* h) {
  if (*x >= FB_WIDTH || *y >= FB_HEIGHT)
    return false;
  if (*x + *w >= FB_WIDTH)
    *w = FB_WIDTH - *x;
  if (*y + *h >= FB_HEIGHT)
    *h = FB_HEIGHT - *y;
  return true;
}

void draw_rect(u32 x, u32 y, u32 w, u32 h, u32 color) {
  u32 w1;

  if (!can_draw(&x, &y, &w, &h))
    return;

  for (; h-- > 0; ++y) {
    for (w1 = 0; w1 < w; ++w1)
      fb[(x + w1) + y * FB_WIDTH] = color;
  }
}

/*
static inline void draw_char(u32 x, u32 y, u32 w, u32 h, u32 scale, u32 color, char c) {
  u32 w1, h1;
  char c1;
  const char* char_data;

  if (!can_draw(&x, &y, &w, &h))
    return;

  char_data = __font[(u8)c];
  for (h1 = 0; h1 < h; ++h1) {
    c1 = char_data[h1 / scale];
    for (w1 = 0; w1 < w; ++w1) {
      if ((c1 << (w1 / scale)) & (1 << (FONT_WIDTH - 1)))
        fb[(x + w1) + (y + h1) * FB_WIDTH] = color;
    }
  }
}

void draw_text(u32 x, u32 y, u32 scale, u32 color, const char* text) {
  u32 w1, w, h, text_w, end, l;

  l = strlen(text);

  h = FONT_HEIGHT * scale;
  w = FONT_WIDTH * scale;

  text_w = w * l;

  if (!can_draw(&x, &y, &text_w, &h))
    return;

  end = x + text_w;
  while (*text && x < end) {
    w1 = end - x;
    w1 = MIN(w1, w);
    draw_char(x, y, w1, h, scale, color, *(text++));
    x += w;
  }
}
*/

void write_text_with_color(u32 scale, u32 color, const char* text) {
  u32 start_x;
  u32 x, y;
  u32 w1, w;
  u32 h1, h;
  char c1, c;
  const char* char_data;

  h = FONT_HEIGHT * scale;
  w = FONT_WIDTH * scale;

  start_x = __cur.x;
  while ((c = *(text++))) {
    if (c == '\n') {
      set_cursor_y(__cur.y + h);
      continue;
    }
    if (c == '\r') {
      set_cursor_x(start_x);
      continue;
    }

    char_data = __font[(u8)c];
    for (h1 = 0; h1 < h; ++h1) {
      c1 = char_data[h1 / scale];
      for (w1 = 0; w1 < w; ++w1) {
        if ((c1 << (w1 / scale)) & (1 << (FONT_WIDTH - 1))) {
          x = (__cur.x + w1);
          y = (__cur.y + h1);
          if (x < FB_WIDTH && y < FB_HEIGHT)
            fb[x + y * FB_WIDTH] = color;
        }
      }
    }
    set_cursor_x(__cur.x + w);
  }
}

void draw_button(u32 x, u32 y, struct button* b) {
  u32 bg_color, fg_color;
  u32 orig_x, orig_y;

  orig_x = get_cursor_x();
  orig_y = get_cursor_y();

  set_cursor_x(x);
  set_cursor_y(y);

  if (b->state == BUTTON_HOVER) {
    bg_color = __fg_color;
    fg_color = __bg_color;
  } else {
    bg_color = __bg_color;
    fg_color = __fg_color;
  }

  draw_rect(b->tl.x - 2, b->tl.y - 2, b->br.x - b->tl.x + 4, b->br.y - b->tl.y + 4, bg_color);

  b->tl.x = x;
  b->tl.y = y;

  write_text_with_color(1, fg_color, b->text);

  b->br.x = get_cursor_x();
  b->br.y = get_cursor_y() + FONT_HEIGHT * 1;

  set_cursor_x(orig_x);
  set_cursor_y(orig_y);
}
