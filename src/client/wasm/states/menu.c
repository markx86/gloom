#include <client.h>
#include <ui.h>

#define FOREGROUND_COLOR SOLIDCOLOR(LIGHTGRAY)
#define BACKGROUND_COLOR SOLIDCOLOR(BLACK)

static void on_play_clicked(void) { switch_to_state(STATE_LOADING); }
static void on_options_clicked(void) { switch_to_state(STATE_OPTIONS); }
static void on_about_clicked(void) { switch_to_state(STATE_ABOUT); }

static struct component comps[] = {
  { .type = UICOMP_BUTTON, .text = "> play", .on_click = on_play_clicked },
  { .type = UICOMP_BUTTON, .text = "> options", .on_click = on_options_clicked },
  { .type = UICOMP_BUTTON, .text = "> about", .on_click = on_about_clicked }
};

static void on_tick(f32 delta) {
  u32 i;

  UNUSED(delta);

  // render buttons menu
  for (i = 0; i < ARRLEN(comps); ++i)
    draw_component(48, 32 + TITLE_HEIGHT + 24 * i, comps + i);
}

static void on_enter(void) {
  if (pointer_is_locked())
    pointer_release();

  if (in_game())
    leave_game();

  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  clear_screen();
  draw_title(32, 32, "gloom");
  component_on_enter(comps, ARRLEN(comps));
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  component_on_mouse_moved(x, y, dx, dy, comps, ARRLEN(comps));
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
