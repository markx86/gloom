#include <client.h>
#include <ui.h>

#define FOREGROUND_COLOR 0xFFAAAAAA
#define BACKGROUND_COLOR 0xFF000000

static void on_play_clicked(void) { switch_to_state(STATE_GAME); }
static void on_options_clicked(void) { switch_to_state(STATE_OPTIONS); }

static struct component comps[] = {
  { .type = UICOMP_BUTTON, .text = "> play", .on_click = on_play_clicked },
  { .type = UICOMP_BUTTON, .text = "> options", .on_click = on_options_clicked },
  { .type = UICOMP_BUTTON, .text = "> about" }
};

static void on_tick(f32 delta) {
  u32 i;
  const u32 y = 32 + 16 * 2 * 3;

  UNUSED(delta);

  // render buttons menu
  set_cursor_y(32 + 16 * 2 * 3);
  for (i = 0; i < ARRLEN(comps); ++i)
    draw_component(48, y + 24 * i, comps + i);
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

  component_on_enter(comps, ARRLEN(comps));
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
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

const struct state_handlers menu_state = {
  .on_tick = on_tick,
  .on_enter = on_enter,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up
};
