#include <gloom/client.h>
#include <gloom/globals.h>

static b8 should_tick;

static const struct state_handlers* handlers[CLIENT_STATE_MAX] = {
  [CLIENT_ERROR]   = &g_error_state,
  [CLIENT_LOADING] = &g_loading_state,
  [CLIENT_WAITING] = &g_waiting_state,
  [CLIENT_GAME]    = &g_game_state,
  [CLIENT_PAUSE]   = &g_pause_state,
  [CLIENT_OPTIONS] = &g_options_state,
  [CLIENT_OVER]    = &g_over_state
};

#define CALL_STATE_HANDLER(name, ...)                                      \
  do {                                                                     \
    if (_client_state < CLIENT_STATE_MAX && handlers[_client_state]->name) \
      handlers[_client_state]->name(__VA_ARGS__);                          \
  } while (0)

b8 _pointer_locked;

enum client_state _client_state;

void client_switch_state(enum client_state new_state) {
  if (client_get_state() < CLIENT_STATE_MAX) {
    printf("switching client state from %d to %d\n", _client_state, new_state);
    _client_state = new_state;
    CALL_STATE_HANDLER(on_enter);
  }
}

void gloom_set_pointer_locked(b8 locked) {
  _pointer_locked = locked;
  if (!locked) {
    g_keys.all_keys = 0;
    // dirty hack to pause on lost focus because I refuse to add another
    // handler just for pointer lock changes
    if (client_get_state() == CLIENT_GAME) {
      // notify the server the player has stopped
      queue_key_input();
      multiplayer_send_update();
      // switch to pause menu
      client_switch_state(CLIENT_PAUSE);
    }
  }
}

void gloom_on_ws_close(void) {
  multiplayer_set_state(MULTIPLAYER_DISCONNECTED);
  if (client_get_state() != CLIENT_OVER)
    client_switch_state(CLIENT_ERROR);
}

void gloom_on_key_event(u32 code, char ch, b8 pressed) {
  CALL_STATE_HANDLER(on_key, code, ch, pressed);
}

void gloom_on_mouse_down(u32 x, u32 y, u32 button) {
  CALL_STATE_HANDLER(on_mouse_down, x, y, button);
}

void gloom_on_mouse_up(u32 x, u32 y, u32 button) {
  CALL_STATE_HANDLER(on_mouse_up, x, y, button);
}

void gloom_on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  CALL_STATE_HANDLER(on_mouse_moved, x, y, dx, dy);
}

b8 gloom_tick(f32 delta) {
  CALL_STATE_HANDLER(on_tick, delta); return should_tick;
}

void gloom_init(b8 ws_connected, u32 game_id, u32 player_token) {
  _pointer_locked = false;
  should_tick = true;
  g_tracked_sprite_set(NULL);
  g_settings_apply();
  multiplayer_init(game_id, player_token);
  client_switch_state(ws_connected ? CLIENT_LOADING : CLIENT_ERROR);
}

void gloom_exit(void) {
  if (client_pointer_is_locked())
    platform_pointer_release();
  multiplayer_leave();
  should_tick = false;
}
