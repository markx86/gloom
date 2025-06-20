#include <client.h>
#include <globals.h>

static b8 should_tick;

static const struct state_handlers* handlers[STATE_MAX] = {
  [STATE_ERROR]   = &error_state,
  [STATE_MENU]    = &menu_state,
  [STATE_LOADING] = &loading_state,
  [STATE_WAITING] = &waiting_state,
  [STATE_GAME]    = &game_state,
  [STATE_PAUSE]   = &pause_state,
  [STATE_OPTIONS] = &options_state,
  [STATE_ABOUT]   = &about_state,
  [STATE_OVER]    = &over_state
};

#define CALLSTATEHANDLER(name, ...)                                   \
  do {                                                                \
    if (__client_state < STATE_MAX && handlers[__client_state]->name) \
      handlers[__client_state]->name(__VA_ARGS__);                    \
  } while (0)

u32 __fb[FB_WIDTH * FB_HEIGHT];
u32 __alpha_mask;
b8 __pointer_locked;

const u32 __palette[] = {
  [COLOR_BLACK]       = 0x000000,
  [COLOR_GRAY]        = 0x7E7E7E,
  [COLOR_LIGHTGRAY]   = 0xBEBEBE,
  [COLOR_WHITE]       = 0xFFFFFF,
  [COLOR_DARKRED]     = 0x00007E,
  [COLOR_RED]         = 0x0000FE,
  [COLOR_DARKGREEN]   = 0x007E04,
  [COLOR_GREEN]       = 0x04FF06,
  [COLOR_DARKYELLOW]  = 0x007E7E,
  [COLOR_YELLOW]      = 0x04FFFF,
  [COLOR_DARKBLUE]    = 0x7E0000,
  [COLOR_BLUE]        = 0xFF0000,
  [COLOR_DARKMAGENTA] = 0x7E007E,
  [COLOR_MAGENTA]     = 0xFF00FE,
  [COLOR_DARKCYAN]    = 0x7E7E04,
  [COLOR_CYAN]        = 0xFFFF06,
};

enum client_state __client_state;

void switch_to_state(enum client_state new_state) {
  if (get_client_state() < STATE_MAX) {
    printf("switching client state from %d to %d\n", __client_state, new_state);
    __client_state = new_state;
    CALLSTATEHANDLER(on_enter);
  }
}

void set_pointer_locked(b8 locked) {
  __pointer_locked = locked;
  if (!locked)
    keys.all_keys = 0;
  // dirty hack to pause on lost focus because I refuse to add another
  // handler just for pointer lock changes
  if (get_client_state() == STATE_GAME && !locked)
    switch_to_state(STATE_PAUSE);
}

void key_event(u32 code, char ch, b8 pressed) { CALLSTATEHANDLER(on_key, code, ch, pressed); }
void mouse_down(u32 x, u32 y, u32 button) { CALLSTATEHANDLER(on_mouse_down, x, y, button); }
void mouse_up(u32 x, u32 y, u32 button) { CALLSTATEHANDLER(on_mouse_up, x, y, button); }
void mouse_moved(u32 x, u32 y, i32 dx, i32 dy) { CALLSTATEHANDLER(on_mouse_moved, x, y, dx, dy); }
b8 tick(f32 delta) { CALLSTATEHANDLER(on_tick, delta); return should_tick; }

void init(b8 ws_connected, u32 game_id, u32 player_token) {
  __pointer_locked = false;
  should_tick = true;
  tracked_sprite = NULL;
  apply_settings();
  multiplayer_init(game_id, player_token);
  register_fb(__fb, FB_WIDTH, FB_HEIGHT, FB_SIZE);
  switch_to_state(ws_connected ? STATE_MENU : STATE_ERROR);
}

void exit(void) {
  if (pointer_is_locked())
    pointer_release();
  if (in_game())
    leave_game();
  should_tick = false;
}
