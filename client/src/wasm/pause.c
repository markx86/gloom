#include <client.h>
#include <ui.h>

static void on_resume_clicked(void) {
  switch_to_state(STATE_GAME);
}

static void on_quit_clicked(void) {
  switch_to_state(STATE_MENU);
}

static struct button buttons[] = {
  { .text = "> resume", .on_click = on_resume_clicked },
  { .text = "> quit", .on_click = on_quit_clicked }
};

static void on_tick(f32 delta) {
  u32 i;

  UNUSED(delta);

  for (i = 0; i < ARRLEN(buttons); ++i)
    draw_button(32, 32 + 24 * i, buttons + i);
}

static void on_enter(enum client_state prev_state) {
  u32 i;

  UNUSED(prev_state);

  if (pointer_locked)
    pointer_release();

  // darken the screen by decreasing the alpha channel
  for (i = 0; i < FB_LEN; ++i)
    fb[i] = (fb[i] & ~0xFF000000) | 0x88000000;

  ui_on_enter(buttons, ARRLEN(buttons));
}

static void on_mouse_move(u32 x, u32 y, i32 dx, i32 dy) {
  UNUSED(dx);
  UNUSED(dy);

  ui_on_mouse_move(x, y, buttons, ARRLEN(buttons));
}

static void on_mouse_click(void) {
  ui_on_mouse_click(buttons, ARRLEN(buttons));
}

const struct state_handlers pause_state = {
  .on_tick = on_tick,
  .on_enter = on_enter,
  .on_mouse_moved = on_mouse_move,
  .on_mouse_click = on_mouse_click
};
