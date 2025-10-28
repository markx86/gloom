#include <gloom/client.h>
#include <gloom/gloom.h>
#include <gloom/game.h>
#include <gloom/multiplayer.h>
#include <gloom/math.h>
#include <gloom/libc.h>
#include <gloom/ui.h>

#define BACKGROUND_COLOR SOLID_COLOR(BLACK)
#define FOREGROUND_COLOR SOLID_COLOR(LIGHTGRAY)

static f32 g_wait_time, g_timer_start;
static b8 g_ready;

static void on_ready_click(void);
static void on_options_click(void);

static
void on_quit_clicked(void) {
  gloom_exit();
}

static struct component g_buttons[] = {
  [0] = { .type = UICOMP_BUTTON, .on_click = on_ready_click                        },
  [1] = { .type = UICOMP_BUTTON, .on_click = on_options_click, .text = "> options" },
  [2] = { .type = UICOMP_BUTTON, .on_click = on_quit_clicked,  .text = "> quit"    }
};

void set_ready(b8 yes) {
  g_ready = yes;
  g_buttons[0].text = g_ready ? "> ready: yes" : "> ready: no";
  multiplayer_signal_ready(g_ready);
}

void g_wait_time_set(f32 wtime) {
  g_wait_time = wtime;
  g_timer_start = platform_get_time();
}

static
void on_ready_click(void) {
  set_ready(!g_ready);
}

static
void on_options_click(void) {
  set_ready(false);
  client_switch_state(CLIENT_OPTIONS);
}

static inline
void title(void) {
  const char title[] = "waiting";
  ui_draw_rect(32, 32 + (FONT_HEIGHT >> 1),
               TITLE_WIDTH_IMM(title), TITLE_HEIGHT - FONT_HEIGHT,
               BACKGROUND_COLOR);
  ui_draw_title(32, 32, title);
}

static
void on_tick(f32 delta) {
  char time_str[32];
  const char* text;
  f32 time_left;
  u32 y, i;

  UNUSED(delta);

  if (multiplayer_is_in_game()) {
    client_switch_state(CLIENT_GAME);
    return;
  }

  time_left = g_wait_time - (platform_get_time() - g_timer_start);

  if (isposf(time_left)) {
    snprintf(time_str, sizeof(time_str),
             "> starting in %ds", roundf(time_left));
    text = time_str;
  }
  else if (isposf(g_wait_time))
    text = "> starting...";
  else {
    text = "> waiting for players...";
    g_timer_start = platform_get_time();
  }
  game_render();

  title();
  y = 32 + TITLE_HEIGHT;

  ui_draw_rect(48 - 2, y - 2,
               STRING_WIDTH(text) + 4, STRING_HEIGHT + 4,
               BACKGROUND_COLOR);
  ui_draw_string(48, y, text);
  y += STRING_HEIGHT + 8;

  for (i = 0; i < ARRLEN(g_buttons) - 1; ++i)
    ui_draw_component(48, y + i * 24, g_buttons + i);
  ui_draw_component(48, FB_HEIGHT - 48, g_buttons + 2);

  multiplayer_draw_game_id();
}

static
void on_enter(void) {
  set_ready(false);
  color_set_alpha(0x7F);
  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  ui_on_enter(g_buttons, ARRLEN(g_buttons));
  game_init();
}

static
void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  ui_on_mouse_moved(x, y, dx, dy, g_buttons, ARRLEN(g_buttons));
}

static
void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  ui_on_mouse_down(x, y, g_buttons, ARRLEN(g_buttons));
}

static
void on_mouse_up(u32 x, u32 y, u32 button) {
  UNUSED(button);
  ui_on_mouse_up(x, y, g_buttons, ARRLEN(g_buttons));
}

const struct state_handlers g_waiting_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up,
};
