#include <client.h>

u32 fb[FB_WIDTH * FB_HEIGHT];
b8 pointer_locked = false;

extern struct state_handlers menu_state, game_state;

static enum client_state __state;
static struct state_handlers* __handlers[] = {
  [STATE_MENU] = &menu_state,
  [STATE_GAME] = &game_state
};

#define HANDLE(name, ...)                                 \
  do {                                                    \
    if (__state < STATE_MAX && __handlers[__state]->name) \
      __handlers[__state]->name(__VA_ARGS__);             \
  } while (0)

void set_pointer_locked(b8 locked) {
  pointer_locked = locked;
  if (!locked)
    keys = (struct keys) {0};
}

void key_event(u32 code, char ch, b8 pressed) {
  HANDLE(on_key, code, ch, pressed);
}

void mouse_click(void) {
  HANDLE(on_mouse_click);
}

void mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  HANDLE(on_mouse_moved, x, y, dx, dy);
}

void tick(f32 delta) {
  HANDLE(on_tick, delta);
}

void switch_to_state(enum client_state state) {
  __state = state;
  HANDLE(on_enter);
}

void init(void) {
  register_fb(fb, FB_WIDTH, FB_HEIGHT, FB_SIZE);
  switch_to_state(STATE_MENU);
}
