#include <gloom/client.h>
#include <gloom/gloom.h>
#include <gloom/game.h>
#include <gloom/multiplayer.h>
#include <gloom/ui.h>

#define FOREGROUND_COLOR SOLID_COLOR(LIGHTGRAY)
#define BACKGROUND_COLOR SOLID_COLOR(BLACK)

static
void on_resume_clicked(void) {
  client_switch_state(CLIENT_GAME);
}

static
void on_options_clicked(void) {
  client_switch_state(CLIENT_OPTIONS);
}

static
void on_quit_clicked(void) {
  gloom_exit();
}

static struct component g_buttons[] = {
  { .type = UICOMP_BUTTON, .text = "> resume", .on_click = on_resume_clicked   },
  { .type = UICOMP_BUTTON, .text = "> options", .on_click = on_options_clicked },
  { .type = UICOMP_BUTTON, .text = "> quit", .on_click = on_quit_clicked       }
};

static
void title(void) {
  const char title[] = "menu";
  ui_draw_rect(32, 32 + (FONT_HEIGHT >> 1),
               TITLE_WIDTH_IMM(title), TITLE_HEIGHT - FONT_HEIGHT,
               BACKGROUND_COLOR);
  ui_draw_title(32, 32, title);
}

static
void on_tick(f32 delta) {
  u32 i;

  game_tick(delta);

  title();
  for (i = 0; i < ARRLEN(g_buttons); ++i)
    ui_draw_component(48, 32 + TITLE_HEIGHT + (STRING_HEIGHT + 8) * i, g_buttons + i);

  multiplayer_draw_game_id();
}

static
void on_enter(void) {
  if (client_pointer_is_locked())
    platform_pointer_release();

  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);

  /* Darken the screen by decreasing the alpha channel */
  color_set_alpha(0x7F);

  ui_on_enter(g_buttons, ARRLEN(g_buttons));
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

const struct state_handlers g_pause_state = {
  .on_tick = on_tick,
  .on_enter = on_enter,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up
};
