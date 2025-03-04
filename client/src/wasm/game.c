#include <client.h>

#define COLOR_SKY   0xFFFF0000
#define COLOR_FLOOR 0xFF000000
#define COLOR_WALLH 0xFFFFFFFF
#define COLOR_WALLV 0xFFAAAAAA

void draw_column(u8 cell_id, i32 x, const struct hit* hit) {
  i32 y, line_y, line_height, line_color;

  // draw column
  if (cell_id) {
    line_color = hit->vertical ? COLOR_WALLV : COLOR_WALLH;

    line_height = FB_HEIGHT / hit->dist;
    if ((u32)line_height > FB_HEIGHT)
      line_height = FB_HEIGHT;
    line_y = (FB_HEIGHT - line_height) >> 1;
  } else
    line_y = FB_HEIGHT >> 1;

  // fill column
  y = 0;
  for (; y < line_y; ++y) {
    fb[x + y * FB_WIDTH] = COLOR_SKY;
  }
  if (cell_id) {
    line_y += line_height;
    for (; y < line_y; ++y)
      fb[x + y * FB_WIDTH] = line_color;
  }
  for (; y < FB_HEIGHT; ++y)
    fb[x + y * FB_WIDTH] = COLOR_FLOOR;
}

void draw_sprite(struct sprite* s) {
  u32 screen_h;
  i32 x_start, x_end, y_start, y_end;
  i32 x, y;

  screen_h = (f32)s->dim.y * s->camera_depth;

  // determine screen coordinates of the sprite
  x_start = s->screen_x - s->screen_halfw;
  x_end = s->screen_x + s->screen_halfw;
  y_end = (f32)(FB_HEIGHT >> 1) * (1.0f + s->camera_depth);
  y_start = y_end - screen_h;
  // draw the sprite
  for (x = MAX(0, x_start); x < x_end && x < FB_WIDTH; x++) {
    if (z_buf[x] < s->dist_from_player2)
      continue;
    for (y = MAX(0, y_start); y < y_end && y < FB_HEIGHT; y++)
      fb[x + y * FB_WIDTH] = s->color;
  }
}

static void on_enter(void) {
  if (!pointer_locked)
    pointer_lock();
  set_camera_fov(DEG2RAD(CAMERA_FOV));
  set_player_rot(0);
  camera.dof = CAMERA_DOF;
}

static void on_key(u32 code, char ch, b8 pressed) {
  if (!pointer_locked)
    return;
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
    case KEY_Q:
      switch_to_state(STATE_MENU);
      break;
    default:
      break;
  }

  UNUSED(ch);
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  if (!pointer_locked)
    return;

  off_player_rot(dx * PLAYER_ROT_SPEED);

  UNUSED(x);
  UNUSED(y);
  UNUSED(dy);
}

static void on_mouse_click(void) {
  if (!pointer_locked) {
    pointer_lock();
    return;
  }
}

struct state_handlers game_state = {
  .on_tick = game_tick,
  .on_enter = on_enter,
  .on_key = on_key,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_click = on_mouse_click,
};
