#include <client.h>
#include <ui.h>

struct sprite* tracked_sprite;
static b8 dead;

#define FOREGROUND_COLOR (dead ? SOLIDCOLOR(WHITE) : SOLIDCOLOR(BLACK))
#define BACKGROUND_COLOR (dead ? SOLIDCOLOR(RED)   : SOLIDCOLOR(GREEN))

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

static void on_enter(void) {
  if (pointer_is_locked())
    pointer_release();

  dead = player.health == 0;

  set_alpha(0x7F);
  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  component_on_enter(&back_btn, 1);
}

static inline void title(void) {
  const char* title = dead ? "dead" : "you win";
  draw_rect(
    32, 32 + (FONT_HEIGHT >> 1),
    TITLE_WIDTH(title), TITLE_HEIGHT - FONT_HEIGHT,
    BACKGROUND_COLOR);
  draw_title(32, 32, title);
}

static void on_tick(f32 delta) {
  if (dead && tracked_sprite != NULL) {
    player.pos = tracked_sprite->pos;
    set_player_rot(tracked_sprite->rot);
  }
  gloom_tick(delta);

  title();
  draw_component(48, 32 + TITLE_HEIGHT, &back_btn);
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

const struct state_handlers over_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up,
};
