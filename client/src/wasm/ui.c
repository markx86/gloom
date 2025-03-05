#define DEFINE_FONT
#include <ui.h>

#define SLIDER_THICKNESS 8
#define PAD              2

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

static void write_text_with_color(u32* x, u32* y, u32 scale, u32 color, const char* text) {
  u32 start_x;
  u32 px, py;
  u32 w1, w;
  u32 h1, h;
  char c1, c;
  const char* char_data;

  h = FONT_HEIGHT * scale;
  w = FONT_WIDTH * scale;

  start_x = *x;

  while ((c = *(text++))) {
    if (c == '\n') {
      *y += h;
      if (*y >= FB_HEIGHT)
        break;
      continue;
    }
    if (c == '\r') {
      *x = start_x;
      continue;
    } else if (*x >= FB_WIDTH)
      continue;

    char_data = __font[(u8)c];
    for (h1 = 0; h1 < h; ++h1) {
      c1 = char_data[h1 / scale];
      for (w1 = 0; w1 < w; ++w1) {
        if ((c1 << (w1 / scale)) & (1 << (FONT_WIDTH - 1))) {
          px = (*x + w1);
          py = (*y + h1);
          if (px < FB_WIDTH && py < FB_HEIGHT)
            fb[px + py * FB_WIDTH] = color;
        }
      }
    }
    *x += w;
  }
}

void draw_component(u32 x, u32 y, struct component* c) {
  u32 bg_color, fg_color;
  i32 pad;
  char checkbox_tick[] = "[ ]";

  if (c->state != UICOMP_IDLE) {
    bg_color = __fg_color;
    fg_color = __bg_color;
  } else {
    bg_color = __bg_color;
    fg_color = __fg_color;
  }

  draw_rect(c->tl.x, c->tl.y, c->br.x - c->tl.x, c->br.y - c->tl.y, bg_color);

  c->tl.x = x - PAD;
  c->tl.y = y - PAD;

  write_text_with_color(&x, &y, 1, fg_color, c->text);

  if (c->type == UICOMP_CHECKBOX) {
    pad = c->pad - (sizeof(checkbox_tick)-1) * FONT_WIDTH;
    if (pad < 0)
      pad = 0;
    pad += FONT_WIDTH;

    checkbox_tick[1] = c->ticked ? 'x' : ' ';
    x += pad;
    write_text_with_color(&x, &y, 1, fg_color, checkbox_tick);
  }

  c->br.x = x;

  if (c->type == UICOMP_SLIDER) {
    pad = c->pad - SLIDER_WIDTH - PAD;
    if (pad < 0)
      pad = 0;
    pad += FONT_WIDTH;

    draw_rect(
      c->br.x + pad, y + ((FONT_HEIGHT - SLIDER_THICKNESS) >> 1),
      SLIDER_WIDTH * c->value, SLIDER_THICKNESS,
      fg_color);
    c->br.x += pad + SLIDER_WIDTH + PAD;
  }

  c->br.x += PAD;
  c->br.y = y + FONT_HEIGHT + PAD;
}

void draw_string(u32 x, u32 y, const char* text) {
  write_text_with_color(&x, &y, 1, __fg_color, text);
}

void draw_title(u32 x, u32 y, const char* text) {
  char pad[128];
  u32 end, xx;

  end = strlen(text) + 2;
  // padding does not fit into buffer, return
  if (end + 2 >= sizeof(pad))
    return;
  pad[end + 2] = '\0';

  memset(pad + 1, '\xd0', end++);

  pad[0] = '\xd2';
  pad[end] = '\xd3';
  xx = x;
  write_text_with_color(&xx, &y, 2, __fg_color, pad);
  y += FONT_HEIGHT << 1;

  xx = x;
  write_text_with_color(&xx, &y, 2, __fg_color, "\xd1 ");
  write_text_with_color(&xx, &y, 2, __fg_color, text);
  write_text_with_color(&xx, &y, 2, __fg_color, " \xd1");
  y += FONT_HEIGHT << 1;

  pad[0] = '\xd4';
  pad[end] = '\xd5';
  xx = x;
  write_text_with_color(&xx, &y, 2, __fg_color, pad);
}
