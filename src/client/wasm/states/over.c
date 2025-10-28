#include <gloom/client.h>
#include <gloom/ui.h>

static struct sprite* tracked_sprite;
static b8 dead;

#define FOREGROUND_COLOR (dead ? SOLID_COLOR(WHITE) : SOLID_COLOR(BLACK))
#define BACKGROUND_COLOR (dead ? SOLID_COLOR(RED)   : SOLID_COLOR(GREEN))

void g_tracked_sprite_set(struct sprite* sprite) {
  tracked_sprite = sprite;
}

struct sprite* g_tracked_sprite_get(void) {
  return tracked_sprite;
}

static
void on_back_clicked(void) {
  gloom_exit();
}

static struct component back_btn = {
  .type = UICOMP_BUTTON,
  .text = "> back",
  .on_click = on_back_clicked
};

static
void on_enter(void) {
  if (client_pointer_is_locked())
    platform_pointer_release();

  dead = g_player.health == 0;

  color_set_alpha(0x7F);
  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  ui_on_enter(&back_btn, 1);
}

static inline
void title(void) {
  const char* title = dead ? "dead" : "you win";
  ui_draw_rect(
    32, 32 + (FONT_HEIGHT >> 1),
    TITLE_WIDTH(title), TITLE_HEIGHT - FONT_HEIGHT,
    BACKGROUND_COLOR);
  ui_draw_title(32, 32, title);
}

static
void on_tick(f32 delta) {
  if (dead && tracked_sprite != NULL) {
    g_player.pos = tracked_sprite->pos;
    game_set_player_rot(tracked_sprite->rot);
  }
  game_tick(delta);

  title();
  ui_draw_component(48, 32 + TITLE_HEIGHT, &back_btn);
}

static
void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  ui_on_mouse_moved(x, y, dx, dy, &back_btn, 1);
}

static
void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  ui_on_mouse_down(x, y, &back_btn, 1);
}

static
void on_mouse_up(u32 x, u32 y, u32 button) {
  UNUSED(button);
  ui_on_mouse_up(x, y, &back_btn, 1);
}

const struct state_handlers g_over_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up,
};
