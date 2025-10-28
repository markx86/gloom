#include <gloom/client.h>
#include <gloom/globals.h>
#include <gloom/game.h>
#include <gloom/multiplayer.h>
#include <gloom/libc.h>

static const struct state_handlers* handlers[CLIENT_STATE_MAX] = {
  [CLIENT_ERROR]   = &g_error_state,
  [CLIENT_LOADING] = &g_loading_state,
  [CLIENT_WAITING] = &g_waiting_state,
  [CLIENT_GAME]    = &g_game_state,
  [CLIENT_PAUSE]   = &g_pause_state,
  [CLIENT_OPTIONS] = &g_options_state,
  [CLIENT_OVER]    = &g_over_state
};

#define CALL_STATE_HANDLER(name, ...)                                          \
  do {                                                                         \
    if (_g_client_state < CLIENT_STATE_MAX && handlers[_g_client_state]->name) \
      handlers[_g_client_state]->name(__VA_ARGS__);                            \
  } while (0)

static b8 g_should_tick;
b8 _g_pointer_locked;
enum client_state _g_client_state;

void client_switch_state(enum client_state new_state) {
  if (client_get_state() < CLIENT_STATE_MAX) {
    printf("switching client state from %d to %d\n", _g_client_state, new_state);
    _g_client_state = new_state;
    CALL_STATE_HANDLER(on_enter);
  }
}

void gloom_set_pointer_locked(b8 locked) {
  _g_pointer_locked = locked;
  if (!locked) {
    g_keys.all_keys = 0;
    /* Dirty hack to pause the game on lost focus, because I refuse to add
     * another handler just for pointer lock changes.
     */
    if (client_get_state() == CLIENT_GAME) {
      /* Notify the server the player has stopped */
      multiplayer_queue_key_input();
      multiplayer_send_update();
      /* Switch to pause menu */
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
  CALL_STATE_HANDLER(on_tick, delta); return g_should_tick;
}

void gloom_init(b8 ws_connected, u32 game_id, u32 player_token) {
  _g_pointer_locked = false;
  g_should_tick = true;
  g_tracked_sprite = NULL;
  g_settings_apply();
  multiplayer_init(game_id, player_token);
  client_switch_state(ws_connected ? CLIENT_LOADING : CLIENT_ERROR);
}

void gloom_exit(void) {
  if (client_pointer_is_locked())
    platform_pointer_release();
  multiplayer_leave();
  g_should_tick = false;
}
