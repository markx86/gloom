#include <client.h>
#include <globals.h>
#include <ui.h>

#define FOREGROUND_COLOR SOLIDCOLOR(YELLOW)
#define BACKGROUND_COLOR SOLIDCOLOR(BLUE)

static void save_settings(void);

static void on_back_clicked(void) {
  apply_settings();
  save_settings();
  switch_to_state(in_game() ? STATE_PAUSE : STATE_MENU);
}

enum option_control {
  BACK_BUTTON,
#ifdef UNFINISHED_FEATURES
  SOUND_CHECKBOX,
  VOLUME_SLIDER,
#endif
  FOV_SLIDER,
  DRAWDIST_SLIDER,
  MOUSESENS_SLIDER,
  CAMERA_SMOOTHING
};

static struct component comps[] = {
  [BACK_BUTTON]      = { .type = UICOMP_BUTTON,   .text = "> back", .on_click = on_back_clicked },
#ifdef UNFINISHED_FEATURES
  [SOUND_CHECKBOX]   = { .type = UICOMP_CHECKBOX, .text = "> sound", .ticked = true },
  [VOLUME_SLIDER]    = { .type = UICOMP_SLIDER,   .text = "> volume", .value = 1.0f },
#endif
  [FOV_SLIDER]       = { .type = UICOMP_SLIDER,   .text = "> field of view", .value = 0.5f },
  [DRAWDIST_SLIDER]  = { .type = UICOMP_SLIDER,   .text = "> draw distance", .value = 0.66f },
  [MOUSESENS_SLIDER] = { .type = UICOMP_SLIDER,   .text = "> mouse sensitivity", .value = 0.5f },
  [CAMERA_SMOOTHING] = { .type = UICOMP_CHECKBOX, .text = "> camera smoothing", .ticked = true },
};

static void save_settings(void) {
  // save settings to local storage
  store_settings(comps[DRAWDIST_SLIDER].value,
                 comps[FOV_SLIDER].value,
                 comps[MOUSESENS_SLIDER].value,
                 comps[CAMERA_SMOOTHING].ticked);
}

// return slider value between min and max
// FIXME: should this be in the ui module?
static inline f32 slider_value(enum option_control ctrl, f32 min, f32 max) {
  return min + (max - min) * comps[ctrl].value;
}

void apply_settings(void) {
  camera.dof = slider_value(DRAWDIST_SLIDER, MIN_CAMERA_DOF, MAX_CAMERA_DOF);
  camera.smoothing = comps[CAMERA_SMOOTHING].ticked;
  set_camera_fov(DEG2RAD(slider_value(FOV_SLIDER, MAX_CAMERA_FOV, MIN_CAMERA_FOV)));
  mouse_sensitivity = slider_value(MOUSESENS_SLIDER, MIN_MOUSE_SENS, MAX_MOUSE_SENS);
}

void load_settings(f32 drawdist, f32 fov, f32 mousesens, b8 camsmooth) {
  comps[DRAWDIST_SLIDER].value = drawdist;
  comps[FOV_SLIDER].value = fov;
  comps[MOUSESENS_SLIDER].value = mousesens;
  comps[CAMERA_SMOOTHING].ticked = camsmooth != 0;
  apply_settings();
}

static void on_enter(void) {
  u32 i;
  struct component* c;

  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  clear_screen();
  draw_title(32, 32, "options");

  // initialize ui components
  component_on_enter(comps, ARRLEN(comps));
  for (i = 1; i < ARRLEN(comps); ++i) {
    c = &comps[i];
    if (c->type != UICOMP_BUTTON)
      c->pad = FB_WIDTH - (48 << 1) - STRING_WIDTH(c->text);
  }
}

static void on_tick(f32 delta) {
  u32 i;

  UNUSED(delta);

  // draw ui components
  for (i = 1; i < ARRLEN(comps); ++i)
    draw_component(48, 32 + TITLE_HEIGHT + (STRING_HEIGHT + 8) * (i-1), comps + i);
  draw_component(48, FB_HEIGHT - 32 - STRING_HEIGHT, &comps[BACK_BUTTON]);
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

const struct state_handlers options_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up
};
