#include <client.h>
#include <ui.h>

#define BACKGROUND_COLOR SOLIDCOLOR(BLACK)
#define FOREGROUND_COLOR SOLIDCOLOR(LIGHTGRAY)

f32 wait_time;
static f32 timer_start;

#ifdef UNFINISHED_FEATURES
static void on_back_clicked(void) {
  switch_to_state(STATE_MENU);
}
#else
static void on_ready_click(void);
static void on_options_click(void) { switch_to_state(STATE_OPTIONS); }
static void on_quit_clicked(void) { exit(); }
#endif

#ifdef UNFINISHED_FEATURES
static struct component back_btn = {
  .type = UICOMP_BUTTON,
  .text = "> back",
  .on_click = on_back_clicked
};
#else
static struct component comps[] = {
  [0] = { .type = UICOMP_BUTTON, .on_click = on_ready_click },
  [1] = { .type = UICOMP_BUTTON, .text = "> options", .on_click = on_options_click },
  [2] = { .type = UICOMP_BUTTON, .text = "> quit", .on_click = on_quit_clicked }
};

static b8 ready;

static void set_ready(b8 yes) {
  ready = yes;
  comps[0].text = ready ? "> ready: yes" : "> ready: no";
}

static void on_ready_click(void) {
  set_ready(!ready);
  signal_ready(ready);
}
#endif

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
#ifndef UNFINISHED_FEATURES
  u32 i;
#endif

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
#ifdef UNFINISHED_FEATURES
  draw_component(48, y, &back_btn);
#else
  for (i = 0; i < ARRLEN(comps); ++i)
    draw_component(48, y + i * 24, comps + i);
#endif

  display_game_id();
}

static void on_enter(void) {
  set_alpha(0x7F);
  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  component_on_enter(comps, ARRLEN(comps));
  set_ready(false);
  gloom_init();
  timer_start = time();
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

const struct state_handlers waiting_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up,
};
