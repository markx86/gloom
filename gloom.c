#include <types.h>
#include <env.h>
#include <libc.h>
#include <math.h>

#define SKY_COLOR   0xFFFF0000
#define FLOOR_COLOR 0xFF000000
#define WALLH_COLOR 0xFFFFFFFF
#define WALLV_COLOR 0xFFAAAAAA

#define PLAYER_SPEED 3.0f;
#define COLLISION_LOOKAHEAD 0.15f

struct camera {
  u32 dof;
  f32 fov;
  f32 plane_halfw;
  vec2f plane;
  vec2f plane_norm;
};

struct actor {
  vec2i pos;
  vec2f dpos;
  f32 rot;
  vec2f dir;
};

struct sprite {
  vec2i pos;
  vec2f dpos;
  vec2i dim;
};

struct map {
  u32 w, h;
  u8 data[];
};

struct keys {
  b8 forward;
  b8 backward;
  b8 right;
  b8 left;
};

static b8 pointer_locked = false;

static struct camera camera;

static struct actor player = {
  .pos = {
    .x = 2,
    .y = 2,
  },
  .dpos = {
    .x = 0.5f,
    .y = 0.5f,
  },
};

static struct map map = {
  .w = 12,
  .h = 15,
  .data = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  },
};

static i32 fb_width, fb_height;
static u32 fb_size, *fb;
static f32* z_buf;

static struct keys keys;

static struct sprite sprites[] = {
  [0] = {
    .pos = {
      .x = 5,
      .y = 5
    },
    .dpos = {
      .x = 0.5f,
      .y = 0.25f,
    },
    .dim = {
      .x = 128,
      .y = 480,
    }
  }
};

// NOTE @new_fov must be in radians
static void set_camera_fov(f32 new_fov) {
  camera.fov = new_fov;
  camera.plane_halfw = 1.0f / (2.0f * tan(new_fov / 2.0f));
}

// NOTE @new_rot must be in radians
static void set_player_rot(f32 new_rot) {
  player.rot = new_rot;
  // compute player versor
  player.dir.x = cos(new_rot);
  player.dir.y = sin(new_rot);
  // compute camera plane vector
  camera.plane_norm.x = -player.dir.y;
  camera.plane_norm.y = +player.dir.x;
  // compute camera plane offset
  camera.plane = VEC2SCALE(camera.plane_norm, camera.plane_halfw);
}

static inline void off_player_rot(f32 delta) {
  f32 new_rot = player.rot + delta;
  if (new_rot >= TWO_PI)
    new_rot -= TWO_PI;
  else if (new_rot < 0.0f)
    new_rot += TWO_PI;
  set_player_rot(new_rot);
}

static void clear_screen(u32 color) {
  u32 i;
  for (i = 0; i < fb_size; ++i)
    fb[i] = color;
}

static void render_scene(void) {
  vec2f ray_dir, delta_dist, dist, intersec_dist;
  vec2i step_dir;
  vec2u map_coords;
  f32 cam_x, ray_dist;
  u32 cell_id, map_off, d;
  i32 x, y;
  i32 line_y, line_height, line_color;
  b8 vertical;

  for (x = 0; x < fb_width; ++x) {
    // cast ray
    {
      cam_x = (2.0f * ((f32)x / fb_width)) - 1.0f;

      // we do not use VEC2* macros, because this is faster
      ray_dir.x = player.dir.x + camera.plane.x * cam_x;
      ray_dir.y = player.dir.y + camera.plane.y * cam_x;

      delta_dist.y = abs(1.0f / ray_dir.x);
      delta_dist.x = abs(1.0f / ray_dir.y);

      dist.x = ray_dir.x > 0 ? (1.0f - player.dpos.x) : player.dpos.x;
      dist.y = ray_dir.y > 0 ? (1.0f - player.dpos.y) : player.dpos.y;

      intersec_dist.y = delta_dist.y * dist.x;
      intersec_dist.x = delta_dist.x * dist.y;

      step_dir.x = SIGN(ray_dir.x);
      step_dir.y = SIGN(ray_dir.y);

      map_coords = REINTERPRET(player.pos, vec2u);

      vertical = true;
      cell_id = 0;
      for (d = 0; d < camera.dof; ++d) {
        if (map_coords.x >= map.w || map_coords.y >= map.h)
          break;

        map_off = map_coords.x + map_coords.y * map.w;
        if ((cell_id = map.data[map_off]))
          break;

        if (intersec_dist.y < intersec_dist.x) {
          intersec_dist.y += delta_dist.y;
          map_coords.x += step_dir.x;
          vertical = true;
        } else {
          intersec_dist.x += delta_dist.x;
          map_coords.y += step_dir.y;
          vertical = false;
        }
      }
    }

    // draw column
    if (cell_id) {
      if (vertical) {
        ray_dist = intersec_dist.y - delta_dist.y;
        line_color = WALLV_COLOR;
      } else {
        ray_dist = intersec_dist.x - delta_dist.x;
        line_color = WALLH_COLOR;
      }
      z_buf[x] = ray_dist * ray_dist; // dist^2

      line_height = fb_height / ray_dist;
      if (line_height > fb_height)
        line_height = fb_height;
      line_y = (fb_height - line_height) >> 1;
    } else {
      z_buf[x] = 1e10;
      line_y = fb_height >> 1;
    }

    // fill column
    y = 0;
    for (; y < line_y; ++y)
      fb[x + y * fb_width] = SKY_COLOR;
    if (cell_id) {
      line_y += line_height;
      for (; y < line_y; ++y)
        fb[x + y * fb_width] = line_color;
    }
    for (; y < fb_height; ++y)
      fb[x + y * fb_width] = FLOOR_COLOR;
  }
}

static inline void compute_inverse_matrix(f32* m11, f32* m12, f32* m21, f32* m22) {
  f32 c = 1.0f / (camera.plane_norm.x * player.dir.y - camera.plane_norm.y * player.dir.x);
  *m11 = +player.dir.y * c;
  *m12 = -player.dir.x * c;
  *m21 = -camera.plane_norm.y * c;
  *m22 = +camera.plane_norm.x * c;
}

static void render_sprites(void) {
  vec2f diff, proj;
  u32 i;
  u32 w, h;
  i32 x_start, x_end, y_start, y_end;
  i32 screen_x, x, y;
  f32 m11, m12, m21, m22;
  f32 dist_from_player2, inv_dist_from_player;
  struct sprite* s = sprites;

  compute_inverse_matrix(&m11, &m12, &m21, &m22);
  for (i = 0; i < ARRLEN(sprites); ++i) {
    diff.x = (f32)(s->pos.x - player.pos.x) + (s->dpos.x - player.dpos.x);
    diff.y = (f32)(s->pos.y - player.pos.y) + (s->dpos.y - player.dpos.y);
    dist_from_player2 = diff.x * diff.x + diff.y * diff.y;
    inv_dist_from_player = inv_sqrt(dist_from_player2);

    proj.x = m11 * diff.x + m12 * diff.y;
    proj.y = m21 * diff.x + m22 * diff.y;

    // sprite is behind the camera, ignore it
    if (proj.y < 0.0f)
      goto end_loop;

    screen_x = fb_width * (0.5f + proj.x / proj.y);
    w = (f32)s->dim.x * inv_dist_from_player;
    h = (f32)s->dim.y * inv_dist_from_player;

    // determine screen coordinates of the sprite
    x_start = screen_x - (w >> 1);
    x_end = x_start + w;
    y_end = (f32)(fb_height >> 1) * (1.0f + inv_dist_from_player);
    y_start = y_end - h;
    for (x = MAX(0, x_start); x < x_end && x < (i32)fb_width; x++) {
      if (z_buf[x] < dist_from_player2)
        continue;
      for (y = MAX(0, y_start); y < y_end && y < (i32)fb_height; y++) {
        fb[x + y * fb_width] = 0xFF0000FF;
      }
    }

end_loop:
    ++s;
  }
}

static void render(void) {
  render_scene();
  render_sprites();
}

static void compute_player_position(f32 space, i32 long_dir, i32 side_dir, vec2i* pos, vec2f* dpos) {
  dpos->x += player.dir.x * long_dir * space;
  dpos->y += player.dir.y * long_dir * space;

  dpos->x += -player.dir.y * side_dir * space;
  dpos->y += +player.dir.x * side_dir * space;

  while (dpos->x >= 1.0f) {
    ++pos->x;
    dpos->x -= 1.0f;
  }
  while (dpos->x < 0.0f) {
    --pos->x;
    dpos->x += 1.0f;
  }

  while (dpos->y >= 1.0f) {
    ++pos->y;
    dpos->y -= 1.0f;
  }
  while (dpos->y < 0.0f) {
    --pos->y;
    dpos->y += 1.0f;
  }
}

static inline void update_player_position(f32 delta) {
  vec2i new_pos, pos;
  vec2f new_dpos, dpos;
  i32 long_dir = keys.forward - keys.backward;
  i32 side_dir = keys.right - keys.left;
  f32 space = delta * PLAYER_SPEED;
  b8 collision = false;

  new_pos = pos = player.pos;
  new_dpos = dpos = player.dpos;

  // account for stupid triangle stuff
  if (long_dir && side_dir)
    space *= INVSQRT2;

  // compute collision position
  compute_player_position(space + COLLISION_LOOKAHEAD, long_dir, side_dir, &pos, &dpos);

  // compute new position
  compute_player_position(space, long_dir, side_dir, &new_pos, &new_dpos);

  // check for x-axis collision
  if (map.data[pos.y * map.w + player.pos.x]) {
    new_pos.y = player.pos.y;
    new_dpos.y = player.dpos.y;
    collision = true;
  }
  // check for y-axis collision
  if (map.data[player.pos.y * map.w + pos.x]) {
    new_pos.x = player.pos.x;
    new_dpos.x = player.dpos.x;
    collision = true;
  }
  // check for diagonal collision if no axis collision was detected
  if (!collision && map.data[pos.y * map.w + pos.x])
    return;

  // update player position
  player.pos = new_pos;
  player.dpos = new_dpos;
}

static void update(f32 delta) {
  update_player_position(delta);
}

void set_pointer_locked(b8 locked) {
  pointer_locked = locked;
}

void key_event(u32 code, char ch, b8 pressed) {
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
    default:
      break;
  }

  UNUSED(ch);
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
  off_player_rot(dx * 0.01f);

  UNUSED(x);
  UNUSED(y);
  UNUSED(dy);
}

void init(u32 w, u32 h) {
  fb_width = w;
  fb_height = h;
  fb_size = fb_width * fb_height;
  fb = malloc(fb_size * sizeof(u32));
  z_buf = malloc(fb_width * sizeof(f32));
  clear_screen(0xFFFF0000);
  set_camera_fov(DEG2RAD(90.0f));
  set_player_rot(0);
  camera.dof = 32;
}

void tick(f32 delta) {
  update(delta);
  render();
}
