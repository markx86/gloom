#include <client.h>

static void on_enter(enum client_state prev_state) {
  if (!pointer_locked)
    pointer_lock();

  if (prev_state != STATE_PAUSE) {
    set_camera_fov(DEG2RAD(CAMERA_FOV));
    set_player_rot(0);
    camera.dof = CAMERA_DOF;
  }

  set_alpha(0xFF);
}

static void on_key(u32 code, char ch, b8 pressed) {
  u32 prev_keys;

  if (!pointer_locked)
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
    send_update();

  UNUSED(ch);
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  if (!pointer_locked)
    return;

  off_player_rot(dx * PLAYER_ROT_SPEED);

  send_update();

  UNUSED(x);
  UNUSED(y);
  UNUSED(dy);
}

static void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  UNUSED(x);
  UNUSED(y);

  if (!pointer_locked) {
    pointer_lock();
    return;
  } else
    fire_bullet();
}

const struct state_handlers game_state = {
  .on_enter = on_enter,
  .on_tick = gloom_tick,
  .on_key = on_key,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
};
