#include <client.h>
#include <ui.h>

#define FOREGROUND_COLOR 0xFF55FFFF
#define BACKGROUND_COLOR 0xFFAA0000

static enum client_state back_state;

static void on_back_clicked(void) { switch_to_state(back_state); }

static struct component comps[] = {
  { .type = UICOMP_CHECKBOX, .text = "> sound ", .pad = 100 },
  { .type = UICOMP_SLIDER,   .text = "> volume", .width = 100 },
  { .type = UICOMP_BUTTON,   .text = "> back", .on_click = on_back_clicked }
};

static void on_enter(enum client_state prev_state) {
  set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);

  clear_screen();

  component_on_enter(comps, ARRLEN(comps));

  back_state = prev_state;
}

static void on_tick(f32 delta) {
  u32 i;

  UNUSED(delta);

  for (i = 0; i < ARRLEN(comps); ++i)
    draw_component(32, 32 + 24 * i, comps + i);
}

static void on_mouse_move(u32 x, u32 y, i32 dx, i32 dy) {
  component_on_mouse_move(x, y, dx, dy, comps, ARRLEN(comps));
}

static void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  component_on_mouse_down(x, y, comps, ARRLEN(comps));
}

static void on_mouse_up(u32 x, u32 y, u32 button) {
  UNUSED(button);
  component_on_mouse_up(x, y, comps, ARRLEN(comps));
}

const struct state_handlers options_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_move,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up
};
