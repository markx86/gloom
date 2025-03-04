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

static void on_enter(void) {
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
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  u32 i;

  UNUSED(dx);
  UNUSED(dy);

  for (i = 0; i < ARRLEN(buttons); ++i)
    buttons[i].state = is_point_over_button(x, y, &buttons[i]) ? BUTTON_HOVER : BUTTON_IDLE;
}

static void on_mouse_click(void) {
  u32 i;

  for (i = 0; i < ARRLEN(buttons); ++i) {
    if (buttons[i].state == BUTTON_HOVER && buttons[i].on_click)
      buttons[i].on_click();
  }
}

struct state_handlers menu_state = {
  .on_tick = on_tick,
  .on_enter = on_enter,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_click = on_mouse_click
};
