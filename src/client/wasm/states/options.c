#include <gloom/client.h>
#include <gloom/globals.h>
#include <gloom/game.h>
#include <gloom/multiplayer.h>
#include <gloom/ui.h>

#define FOREGROUND_COLOR SOLID_COLOR(YELLOW)
#define BACKGROUND_COLOR SOLID_COLOR(BLUE)

static
void save_settings(void);

static
void on_back_clicked(void) {
  g_settings_apply();
  save_settings();
  client_switch_state(multiplayer_is_in_game() ? CLIENT_PAUSE : CLIENT_WAITING);
}

enum option_control {
  BACK_BUTTON,
  FOV_SLIDER,
  DRAWDIST_SLIDER,
  MOUSESENS_SLIDER,
  CAMERA_SMOOTHING
};

static struct component g_comps[] = {
  [BACK_BUTTON]      = { .type = UICOMP_BUTTON,   .text = "> back", .on_click = on_back_clicked },
  [FOV_SLIDER]       = { .type = UICOMP_SLIDER,   .text = "> field of view"                     },
  [DRAWDIST_SLIDER]  = { .type = UICOMP_SLIDER,   .text = "> draw distance"                     },
  [MOUSESENS_SLIDER] = { .type = UICOMP_SLIDER,   .text = "> mouse sensitivity"                 },
  [CAMERA_SMOOTHING] = { .type = UICOMP_CHECKBOX, .text = "> camera smoothing"                  },
};

static
void save_settings(void) {
  /* Save settings to local storage */
  platform_settings_store(g_comps[DRAWDIST_SLIDER].value,
                 g_comps[FOV_SLIDER].value,
                 g_comps[MOUSESENS_SLIDER].value,
                 g_comps[CAMERA_SMOOTHING].ticked);
}

/* Return slider value between min and max.
 * FIXME: Should this be in the ui module?
 */
static inline
f32 slider_value(enum option_control ctrl, f32 min, f32 max) {
  return min + (max - min) * g_comps[ctrl].value;
}

void g_settings_apply(void) {
  f32 new_fov;

  g_camera.dof = slider_value(DRAWDIST_SLIDER, MIN_CAMERA_DOF, MAX_CAMERA_DOF);
  g_camera.smoothing = g_comps[CAMERA_SMOOTHING].ticked;
  g_mouse_sensitivity =
    slider_value(MOUSESENS_SLIDER, MIN_MOUSE_SENS, MAX_MOUSE_SENS);

  new_fov = DEG2RAD(slider_value(FOV_SLIDER, MAX_CAMERA_FOV, MIN_CAMERA_FOV));
  game_camera_set_fov(new_fov);
}

void gloom_settings_load(f32 drawdist, f32 fov, f32 mousesens, b8 camsmooth) {
  g_comps[DRAWDIST_SLIDER].value = drawdist;
  g_comps[FOV_SLIDER].value = fov;
  g_comps[MOUSESENS_SLIDER].value = mousesens;
  g_comps[CAMERA_SMOOTHING].ticked = camsmooth != 0;
  g_settings_apply();
}

void gloom_settings_defaults(void) {
  gloom_settings_load(0.66f, 0.5f, 0.5f, true);
}

static
void on_enter(void) {
  u32 i;
  struct component* c;

  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  ui_clear_screen();
  ui_draw_title(32, 32, "options");

  /* Initialize UI components */
  ui_on_enter(g_comps, ARRLEN(g_comps));
  for (i = 1; i < ARRLEN(g_comps); ++i) {
    c = &g_comps[i];
    if (c->type != UICOMP_BUTTON)
      c->pad = FB_WIDTH - (48 << 1) - STRING_WIDTH(c->text);
  }
}

static
void on_tick(f32 delta) {
  u32 i;
  u32 y;

  UNUSED(delta);

  /* draw UI components */
  for (i = 1; i < ARRLEN(g_comps); ++i) {
    y = 32 + TITLE_HEIGHT + (STRING_HEIGHT + 8) * (i-1);
    ui_draw_component(48, y, g_comps + i);
  }
  ui_draw_component(48, FB_HEIGHT - 32 - STRING_HEIGHT, &g_comps[BACK_BUTTON]);
}

static
void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  ui_on_mouse_moved(x, y, dx, dy, g_comps, ARRLEN(g_comps));
}

static
void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  ui_on_mouse_down(x, y, g_comps, ARRLEN(g_comps));
}

static
void on_mouse_up(u32 x, u32 y, u32 button) {
  UNUSED(button);
  ui_on_mouse_up(x, y, g_comps, ARRLEN(g_comps));
}

const struct state_handlers g_options_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up
};
