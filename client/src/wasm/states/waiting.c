#include <client.h>
#include <ui.h>

#define BACKGROUND_COLOR SOLIDCOLOR(BLACK)
#define FOREGROUND_COLOR SOLIDCOLOR(LIGHTGRAY)

f32 wait_time;

static void on_back_clicked(void) {
  switch_to_state(STATE_MENU);
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
  char time_left[32];
  const char* text;
  u32 y;

  UNUSED(delta);

  if (isposf(wait_time)) {
    snprintf(time_left, sizeof(time_left), "> starting in %ds", (i32)wait_time);
    text = time_left;
  } else
    text = "> waiting for players...";
  gloom_render();

  title();
  y = 32 + TITLE_HEIGHT;
  draw_rect(48 - 2, y - 2, STRING_WIDTH(text) + 4, STRING_HEIGHT + 4, BACKGROUND_COLOR);
  draw_string(48, y, text);
  y += STRING_HEIGHT + 8;
  draw_component(48, y, &back_btn);

  if (wait_time > 0.0f) {
    wait_time -= delta;
    if (wait_time < 0.0f)
      wait_time = 0.0f;
  }
}

static void on_enter(void) {
  set_alpha(0x7F);
  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  component_on_enter(&back_btn, 1);
  gloom_init(DEG2RAD(CAMERA_FOV), CAMERA_DOF);
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
