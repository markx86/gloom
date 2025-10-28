#include <gloom/client.h>
#include <gloom/game.h>
#include <gloom/multiplayer.h>
#include <gloom/globals.h>
#include <gloom/color.h>
#include <gloom/libc.h>

static b8 g_do_send_update;

#define KEY_A 65
#define KEY_D 68
#define KEY_S 83
#define KEY_W 87
#define KEY_P 80

static
void on_enter(void) {
  if (!client_pointer_is_locked())
    platform_pointer_lock();

  color_set_alpha(0xFF);
  g_do_send_update = false;
}

static
void on_tick(f32 delta) {
  if (g_do_send_update) {
    multiplayer_send_update();
    g_do_send_update = false;
  }
  game_tick(delta);
}

static
void on_key(u32 code, char ch, b8 pressed) {
  u32 prev_keys;

  if (!client_pointer_is_locked())
    return;

  prev_keys = g_keys.all_keys;
  switch (code) {
    case KEY_W:
      g_keys.forward = pressed;
      break;
    case KEY_S:
      g_keys.backward = pressed;
      break;
    case KEY_A:
      g_keys.left = pressed;
      break;
    case KEY_D:
      g_keys.right = pressed;
      break;
    case KEY_P:
      client_switch_state(CLIENT_PAUSE);
      return;
    default:
      printf("unhandled key %d (%c)\n", code, ch);
      return;
  }

  if (g_keys.all_keys != prev_keys) {
    multiplayer_queue_key_input();
    g_do_send_update = true;
  }

  UNUSED(ch);
}

static
void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  UNUSED(x);
  UNUSED(y);
  UNUSED(dy);

  if (!client_pointer_is_locked())
    return;

  game_player_add_rot(dx * g_mouse_sensitivity * PLAYER_ROT_SPEED);

  g_do_send_update = true;
}

static
void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  UNUSED(x);
  UNUSED(y);

  if (!client_pointer_is_locked()) {
    platform_pointer_lock();
    return;
  }

  multiplayer_fire_bullet();
}

const struct state_handlers g_game_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_key = on_key,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
};
