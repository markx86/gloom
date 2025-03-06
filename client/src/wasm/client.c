#include <client.h>

u32 fb[FB_WIDTH * FB_HEIGHT];
b8 pointer_locked = false;

extern const struct state_handlers
  menu_state,
  game_state,
  pause_state,
  options_state,
  about_state;

static enum client_state state;
static const struct state_handlers* handlers[STATE_MAX] = {
  [STATE_MENU]    = &menu_state,
  [STATE_GAME]    = &game_state,
  [STATE_PAUSE]   = &pause_state,
  [STATE_OPTIONS] = &options_state,
  [STATE_ABOUT]   = &about_state
};

#define HANDLE(name, ...)                           \
  do {                                              \
    if (state < STATE_MAX && handlers[state]->name) \
      handlers[state]->name(__VA_ARGS__);           \
  } while (0)

void set_pointer_locked(b8 locked) {
  pointer_locked = locked;
  if (!locked)
    keys.all_keys = 0;
  // dirty hack to pause on lost focus because I refuse to add another
  // handler just for pointer lock changes
  if (state == STATE_GAME && !locked)
    switch_to_state(STATE_PAUSE);
}

void key_event(u32 code, char ch, b8 pressed) { HANDLE(on_key, code, ch, pressed); }
void mouse_down(u32 x, u32 y, u32 button) { HANDLE(on_mouse_down, x, y, button); }
void mouse_up(u32 x, u32 y, u32 button) { HANDLE(on_mouse_up, x, y, button); }
void mouse_moved(u32 x, u32 y, i32 dx, i32 dy) { HANDLE(on_mouse_moved, x, y, dx, dy); }
void tick(f32 delta) { HANDLE(on_tick, delta); }

void switch_to_state(enum client_state new_state) {
  enum client_state prev_state = state;
  if (state < STATE_MAX) {
    state = new_state;
    HANDLE(on_enter, prev_state);
  }
}

void init(b8 ws_connected) {
  register_fb(fb, FB_WIDTH, FB_HEIGHT, FB_SIZE);
  switch_to_state(STATE_MENU);
}
