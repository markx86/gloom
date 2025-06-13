#include <client.h>
#include <ui.h>

#define BACKGROUND_COLOR SOLIDCOLOR(BLACK)
#define FOREGROUND_COLOR SOLIDCOLOR(LIGHTGRAY)

f32 wait_time;
static f32 timer_start;

static void on_back_clicked(void) {
#ifdef UNFINISHED_FEATURES
  switch_to_state(STATE_MENU);
#else
  exit();
#endif
}

static struct component back_btn = {
  .type = UICOMP_BUTTON,
  .text = "> back",
  .on_click = on_back_clicked
};

static inline void title(void) {
  const char title[] = "waiting";
  draw_rect(
    32, 32 + (FONT_HEIGHT >> 1),
    TITLE_WIDTH_IMM(title), TITLE_HEIGHT - FONT_HEIGHT,
    BACKGROUND_COLOR);
  draw_title(32, 32, title);
}

static void on_tick(f32 delta) {
  char time_str[32];
  const char* text;
  f32 time_left;
  u32 y;

  UNUSED(delta);

  time_left = wait_time - (time() - timer_start);

  if (isposf(time_left)) {
    snprintf(time_str, sizeof(time_str), "> starting in %ds", roundf(time_left));
    text = time_str;
  }
  else if (isposf(wait_time))
    text = "> starting...";
  else {
    text = "> waiting for players...";
    timer_start = time();
  }
  gloom_render();

  title();
  y = 32 + TITLE_HEIGHT;
  draw_rect(48 - 2, y - 2, STRING_WIDTH(text) + 4, STRING_HEIGHT + 4, BACKGROUND_COLOR);
  draw_string(48, y, text);
  y += STRING_HEIGHT + 8;
  draw_component(48, y, &back_btn);

  display_game_id();
}

static void on_enter(void) {
  set_alpha(0x7F);
  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  component_on_enter(&back_btn, 1);
  gloom_init();
  timer_start = time();
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  component_on_mouse_moved(x, y, dx, dy, &back_btn, 1);
}

static void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  component_on_mouse_down(x, y, &back_btn, 1);
}

static void on_mouse_up(u32 x, u32 y, u32 button) {
  UNUSED(button);
  component_on_mouse_up(x, y, &back_btn, 1);
}

const struct state_handlers waiting_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up,
};
