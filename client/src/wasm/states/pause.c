#include <client.h>
#include <ui.h>

#define FOREGROUND_COLOR SOLIDCOLOR(LIGHTGRAY)
#define BACKGROUND_COLOR SOLIDCOLOR(BLACK)

static void on_resume_clicked(void) {
  switch_to_state(STATE_GAME);
}

static void on_quit_clicked(void) {
  switch_to_state(STATE_MENU);
}

static struct component buttons[] = {
  { .type = UICOMP_BUTTON, .text = "> resume", .on_click = on_resume_clicked },
  { .type = UICOMP_BUTTON, .text = "> quit", .on_click = on_quit_clicked }
};

static void title(void) {
  const char title[] = "paused";
  draw_rect(
    32, 32 + (FONT_HEIGHT >> 1),
    TITLE_WIDTH_IMM(title), TITLE_HEIGHT - FONT_HEIGHT,
    BACKGROUND_COLOR);
  draw_title(32, 32, title);
}

static void on_tick(f32 delta) {
  u32 i;

  gloom_tick(delta);

  title();
  for (i = 0; i < ARRLEN(buttons); ++i)
    draw_component(48, 32 + TITLE_HEIGHT + (STRING_HEIGHT + 8) * i, buttons + i);
}

static void on_enter(void) {
  if (pointer_is_locked())
    pointer_release();

  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);

  // darken the screen by decreasing the alpha channel
  set_alpha(0x7F);

  component_on_enter(buttons, ARRLEN(buttons));
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  component_on_mouse_moved(x, y, dx, dy, buttons, ARRLEN(buttons));
}

static void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  component_on_mouse_down(x, y, buttons, ARRLEN(buttons));
}

static void on_mouse_up(u32 x, u32 y, u32 button) {
  UNUSED(button);
  component_on_mouse_up(x, y, buttons, ARRLEN(buttons));
}

const struct state_handlers pause_state = {
  .on_tick = on_tick,
  .on_enter = on_enter,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up
};
