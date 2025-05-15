#include <client.h>
#include <ui.h>

#define FOREGROUND_COLOR SOLIDCOLOR(YELLOW)
#define BACKGROUND_COLOR SOLIDCOLOR(BLUE)

static void on_back_clicked(void) { switch_to_state(STATE_MENU); }

#define BACK_BUTTON      0
#define SOUND_CHECKBOX   1
#define VOLUME_SLIDER    2
#define DRAWDIST_SLIDER  3
#define MOUSESENS_SLIDER 4

static struct component comps[] = {
  [BACK_BUTTON]      = { .type = UICOMP_BUTTON,   .text = "> back", .on_click = on_back_clicked },
  [SOUND_CHECKBOX]   = { .type = UICOMP_CHECKBOX, .text = "> sound", .ticked = true },
  [VOLUME_SLIDER]    = { .type = UICOMP_SLIDER,   .text = "> volume", .value = 1.0f },
  [DRAWDIST_SLIDER]  = { .type = UICOMP_SLIDER,   .text = "> draw distance", .value = 0.5f },
  [MOUSESENS_SLIDER] = { .type = UICOMP_SLIDER,   .text = "> mouse sensitivity", .value = 0.5f },
};

static void on_enter(void) {
  u32 i;
  struct component* c;

  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);

  clear_screen();

  draw_title(32, 32, "options");

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

  for (i = 1; i < ARRLEN(comps); ++i)
    draw_component(48, 32 + TITLE_HEIGHT + (STRING_HEIGHT + 8) * (i-1), comps + i);

  draw_component(48, FB_HEIGHT - 32 - STRING_HEIGHT, &comps[0]);
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
