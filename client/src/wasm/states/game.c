#include <client.h>

static b8 do_send_update;

static void on_enter(void) {
  if (!pointer_is_locked())
    pointer_lock();

  set_alpha(0xFF);
  do_send_update = false;
}

static void on_tick(f32 delta) {
  if (do_send_update) {
    send_update();
    do_send_update = false;
  }
  gloom_tick(delta);
}

static void on_key(u32 code, char ch, b8 pressed) {
  u32 prev_keys;

  if (!pointer_is_locked())
    return;

  prev_keys = keys.all_keys;
  switch (code) {
    case KEY_W:
      keys.forward = pressed;
      break;
    case KEY_S:
      keys.backward = pressed;
      break;
    case KEY_A:
      keys.left = pressed;
      break;
    case KEY_D:
      keys.right = pressed;
      break;
    case KEY_P:
      switch_to_state(STATE_PAUSE);
      return;
    default:
      printf("unhandled key %d (%c)\n", code, ch);
      return;
  }

  if (keys.all_keys != prev_keys)
    do_send_update = true;

  UNUSED(ch);
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  if (!pointer_is_locked())
    return;

  off_player_rot(dx * PLAYER_ROT_SPEED);

  do_send_update = true;

  UNUSED(x);
  UNUSED(y);
  UNUSED(dy);
}

static void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  UNUSED(x);
  UNUSED(y);

  if (!pointer_is_locked()) {
    pointer_lock();
    return;
  }

  fire_bullet();
}

const struct state_handlers game_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_key = on_key,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
};
