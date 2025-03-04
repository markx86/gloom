#include <gloom.h>

static u32 fb[FB_WIDTH * FB_HEIGHT];

static b8 pointer_locked = false;

static inline void render_scene(void) {
  u8 cell_id;
  i32 x, y;
  i32 line_y, line_height, line_color;
  f32 cam_x;
  vec2f ray_dir;
  struct hit hit;

  for (x = 0; x < FB_WIDTH; ++x) {
    cam_x = (2.0f * ((f32)x / FB_WIDTH)) - 1.0f;

    // we do not use VEC2* macros, because this is faster
    ray_dir.x = player.long_dir.x + camera.plane.x * cam_x;
    ray_dir.y = player.long_dir.y + camera.plane.y * cam_x;

    cell_id = trace_ray(&ray_dir, &hit);

    // draw column
    if (cell_id) {
      line_color = hit.vertical ? COLOR_WALLV : COLOR_WALLH;

      line_height = FB_HEIGHT / hit.dist;
      if ((u32)line_height > FB_HEIGHT)
        line_height = FB_HEIGHT;
      line_y = (FB_HEIGHT - line_height) >> 1;
    } else
      line_y = FB_HEIGHT >> 1;

    z_buf[x] = hit.dist * hit.dist;

    // fill column
    y = 0;
    for (; y < line_y; ++y)
      fb[x + y * FB_WIDTH] = COLOR_SKY;
    if (cell_id) {
      line_y += line_height;
      for (; y < line_y; ++y)
        fb[x + y * FB_WIDTH] = line_color;
    }
    for (; y < FB_HEIGHT; ++y)
      fb[x + y * FB_WIDTH] = COLOR_FLOOR;
  }
}

static inline void render_sprites(void) {
  vec2f proj;
  u32 screen_h;
  u32 i, j, k, n;
  i32 x_start, x_end, y_start, y_end;
  i32 x, y;
  struct sprite* s;
  struct sprite* to_render[MAX_SPRITES_ON_SCREEN];

  n = 0;
  for (i = 0; i < ARRLEN(sprites); ++i) {
    s = sprites + i;

    // compute coordinates in camera space
    proj.x = camera.inv_mat.m11 * s->diff.x + camera.inv_mat.m12 * s->diff.y;
    proj.y = camera.inv_mat.m21 * s->diff.x + camera.inv_mat.m22 * s->diff.y;

    // sprite is behind the camera, ignore it
    if (proj.y < 0.0f)
      continue;

    // save camera depth
    s->camera_depth = 1.0f / proj.y;

    // compute screen x
    s->screen_x = (FB_WIDTH >> 1) * (1.0f + (proj.x / proj.y));
    // compute screen width (we divide by two since we always use the half screen width)
    s->screen_halfw = (i32)((f32)s->dim.x * s->camera_depth) >> 1;

    // sprite is not on screen, ignore it
    if (s->screen_x + s->screen_halfw < 0 || s->screen_x - s->screen_halfw >= FB_WIDTH)
      continue;

    // look for index to insert the new sprite
    for (j = 0; j < n; ++j) {
      if (to_render[j]->dist_from_player2 < s->dist_from_player2)
        break;
    }
    // move elements after the entry to the next index
    for (k = n; k > j; --k)
      to_render[k] = to_render[k-1];
    // store the new sprite to render
    to_render[j] = s;

    if (++n >= MAX_SPRITES_ON_SCREEN)
      break;
  }

  for (i = 0; i < n; i++) {
    s = to_render[i];

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
      for (y = MAX(0, y_start); y < y_end && y < FB_HEIGHT; y++) {
        fb[x + y * FB_WIDTH] = s->color;
      }
    }
  }
}

static void render(void) {
  render_scene();
  render_sprites();
}

void key_event(u32 code, char ch, b8 pressed) {
  if (!pointer_locked)
    return;
  switch (code) {
    case 87:
      keys.forward = pressed;
      break;
    case 83:
      keys.backward = pressed;
      break;
    case 65:
      keys.left = pressed;
      break;
    case 68:
      keys.right = pressed;
      break;
    case 81:
      pointer_release();
      break;
    default:
      break;
  }

  UNUSED(ch);
}

void set_pointer_locked(b8 locked) {
  pointer_locked = locked;
  if (!locked)
    keys = (struct keys) {0};
}

void mouse_click(void) {
  if (!pointer_locked) {
    pointer_lock();
    return;
  }
}

void mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  if (!pointer_locked)
    return;
  off_player_rot(dx * PLAYER_ROT_SPEED);

  UNUSED(x);
  UNUSED(y);
  UNUSED(dy);
}

void init(void) {
  register_fb(fb, FB_WIDTH, FB_HEIGHT, FB_SIZE);
  set_camera_fov(DEG2RAD(CAMERA_FOV));
  set_player_rot(0);
  camera.dof = CAMERA_DOF;
}

void tick(f32 delta) {
  update(delta);
  render();
}
