#define DEFINE_FONT
#include <gloom/ui.h>
#include <gloom/libc.h>

#define SLIDER_THICKNESS 8
#define PAD              2

u32 _g_fg_color = 0xFFFFFFFF, _g_bg_color = 0xFF000000;

static inline
b8 can_draw(u32* x, u32* y, u32* w, u32* h) {
  if (*x >= FB_WIDTH || *y >= FB_HEIGHT)
    return false;
  if (*x + *w >= FB_WIDTH)
    *w = FB_WIDTH - *x;
  if (*y + *h >= FB_HEIGHT)
    *h = FB_HEIGHT - *y;
  return true;
}

void ui_draw_rect(u32 x, u32 y, u32 w, u32 h, u32 color) {
  u32 w1;

  if (!can_draw(&x, &y, &w, &h))
    return;

  for (; h-- > 0; ++y) {
    for (w1 = 0; w1 < w; ++w1)
      fb_set_pixel(x + w1, y, color);
  }
}

static
void write_text_with_color(u32* x, u32* y, u32 scale, u32 color,
                           const char* text) {
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

    char_data = g_font[(u8)c];
    for (h1 = 0; h1 < h; ++h1) {
      c1 = char_data[h1 / scale];
      for (w1 = 0; w1 < w; ++w1) {
        if ((c1 << (w1 / scale)) & (1 << (FONT_WIDTH - 1))) {
          px = (*x + w1);
          py = (*y + h1);
          if (px < FB_WIDTH && py < FB_HEIGHT)
            fb_set_pixel(px, py, color);
        }
      }
    }
    *x += w;
  }
}

void ui_draw_component(u32 x, u32 y, struct component* c) {
  u32 bg_color, fg_color;
  i32 pad;
  char checkbox_tick[] = "[ ]";

  if (c->state != UICOMP_IDLE) {
    bg_color = _g_fg_color;
    fg_color = _g_bg_color;
  } else {
    bg_color = _g_bg_color;
    fg_color = _g_fg_color;
  }

  ui_draw_rect(c->tl.x, c->tl.y, c->br.x - c->tl.x, c->br.y - c->tl.y, bg_color);

  c->tl.x = x - PAD;
  c->tl.y = y - PAD;

  write_text_with_color(&x, &y, 1, fg_color, c->text);

  if (c->type == UICOMP_CHECKBOX) {
    pad = c->pad - (sizeof(checkbox_tick)-1) * FONT_WIDTH;
    pad = MAX(pad, 0) + FONT_WIDTH;

    checkbox_tick[1] = c->ticked ? 'x' : ' ';
    x += pad;
    write_text_with_color(&x, &y, 1, fg_color, checkbox_tick);
  }

  c->br.x = x;

  if (c->type == UICOMP_SLIDER) {
    pad = c->pad - SLIDER_WIDTH - PAD;
    pad = MAX(pad, 0) + FONT_WIDTH;

    ui_draw_rect(
      c->br.x + pad, y + ((FONT_HEIGHT - SLIDER_THICKNESS) >> 1),
      SLIDER_WIDTH * c->value, SLIDER_THICKNESS,
      fg_color);
    c->br.x += pad + SLIDER_WIDTH + PAD;
  }

  c->br.x += PAD;
  c->br.y = y + FONT_HEIGHT + PAD;
}

void ui_draw_string(u32 x, u32 y, const char* text) {
  write_text_with_color(&x, &y, 1, _g_fg_color, text);
}

void ui_draw_string_with_color(u32 x, u32 y, const char* text, u32 color) {
  write_text_with_color(&x, &y, 1, color, text);
}

void ui_draw_title(u32 x, u32 y, const char* text) {
  char pad[32];
  u32 end, xx;

  end = strlen(text) + 2;
  /* Padding does not fit into buffer, return */
  if (end + 2 >= sizeof(pad))
    return;
  pad[end + 2] = '\0';

  memset(pad + 1, '\xd0', end++);

  pad[0] = '\xd2';
  pad[end] = '\xd3';
  xx = x;
  write_text_with_color(&xx, &y, 2, _g_fg_color, pad);
  y += FONT_HEIGHT << 1;

  xx = x;
  write_text_with_color(&xx, &y, 2, _g_fg_color, "\xd1 ");
  write_text_with_color(&xx, &y, 2, _g_fg_color, text);
  write_text_with_color(&xx, &y, 2, _g_fg_color, " \xd1");
  y += FONT_HEIGHT << 1;

  pad[0] = '\xd4';
  pad[end] = '\xd5';
  xx = x;
  write_text_with_color(&xx, &y, 2, _g_fg_color, pad);
}

static inline
b8 component_is_mouse_over(u32 x, u32 y, struct component* c) {
  return (x >= c->tl.x && y >= c->tl.y && x <= c->br.x && y <= c->br.y);
}

void ui_on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy,
                       struct component* comps, u32 n) {
  u32 i;
  b8 interacting;
  struct component* c;

  UNUSED(dy);

  /* If the user is interacting with a ui element, do not process
   * other hover events.
   */
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

void ui_on_mouse_down(u32 x, u32 y, struct component* comps, u32 n) {
  u32 i;
  struct component* c;

  for (i = 0; i < n; ++i) {
    c = &comps[i];
    if (c->state == UICOMP_HOVER && component_is_mouse_over(x, y, c))
      c->state = UICOMP_PRESSED;
  }
}

void ui_on_mouse_up(u32 x, u32 y, struct component* comps, u32 n) {
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

void ui_on_enter(struct component* comps, u32 n) {
  u32 i;
  for (i = 0; i < n; ++i)
    comps[i].state = UICOMP_IDLE;
}
