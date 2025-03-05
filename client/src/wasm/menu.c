#include <client.h>
#include <ui.h>

#define FOREGROUND_COLOR 0xFFAAAAAA
#define BACKGROUND_COLOR 0xFF000000

static void on_play_clicked(void) {
  switch_to_state(STATE_GAME);
}

static void on_options_clicked(void) {
}

static struct button buttons[] = {
  { .text = "> play", .on_click = on_play_clicked },
  { .text = "> options", .on_click = on_options_clicked },
  { .text = "> about" }
};

static void on_tick(f32 delta) {
  u32 i;
  struct button* b;
  const u32 y = 32 + 16 * 2 * 3;

  UNUSED(delta);

  // render buttons menu
  set_cursor_y(32 + 16 * 2 * 3);
  for (i = 0; i < ARRLEN(buttons); ++i) {
    b = buttons + i;
    draw_button(48, y + 24 * i, b);
  }
}

static void on_enter(enum client_state prev_state) {
  UNUSED(prev_state);

  if (pointer_locked)
    pointer_release();

  set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);

  clear_screen();

  // write menu title
  set_cursor_x(32);
  set_cursor_y(32);
  write_text(2,
    "\xd2\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd3\r\n"
    "\xd1 gloom v1.0 \xd1\r\n"
    "\xd4\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd5\r\n"
  );

  ui_on_enter(buttons, ARRLEN(buttons));
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  UNUSED(dx);
  UNUSED(dy);

  ui_on_mouse_move(x, y, buttons, ARRLEN(buttons));
}

static void on_mouse_click(void) {
  ui_on_mouse_click(buttons, ARRLEN(buttons));
}

const struct state_handlers menu_state = {
  .on_tick = on_tick,
  .on_enter = on_enter,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_click = on_mouse_click
};
