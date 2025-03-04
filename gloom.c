#include <types.h>
#include <env.h>
#include <libc.h>
#include <math.h>

#define FB_WIDTH  640
#define FB_HEIGHT 480
#define FB_SIZE   sizeof(fb)
#define FB_LEN    ARRLEN(fb)

#define SKY_COLOR   0xFFFF0000
#define FLOOR_COLOR 0xFF000000
#define WALLH_COLOR 0xFFFFFFFF
#define WALLV_COLOR 0xFFAAAAAA

#define PLAYER_SPEED        3.0f
#define COLLISION_LOOKAHEAD 0.15f

#define MAX_SPRITES_ON_SCREEN 128

struct camera {
  u32 dof;
  f32 fov;
  f32 plane_halfw;
  vec2f plane;
  vec2f plane_norm;
  struct {
    f32 m11, m12, m21, m22;
  } inv_mat;
};

struct actor {
  vec2i pos;
  vec2f dpos;
  f32 rot;
  vec2f dir;
};

struct sprite {
  u32 color;
  vec2i pos;
  vec2f dpos;
  vec2i dim;
  struct {
    i32 screen_x;
    f32 dist_from_player2;
  };
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

static u32 fb[FB_WIDTH * FB_HEIGHT];
static f32 z_buf[FB_WIDTH];

static struct keys keys;

static struct sprite sprites[] = {
  [0] = {
    .color = 0xFF0000FF,
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
  },
  [1] = {
    .color = 0xFF00FF00,
    .pos = {
      .x = 8,
      .y = 7
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
  f32 c;

  player.rot = new_rot;
  // compute player versor
  player.dir.x = cos(new_rot);
  player.dir.y = sin(new_rot);
  // compute camera plane vector
  camera.plane_norm.x = -player.dir.y;
  camera.plane_norm.y = +player.dir.x;
  // compute camera plane offset
  camera.plane = VEC2SCALE(camera.plane_norm, camera.plane_halfw);
  // compute camera inverse matrix
  {
    c = 1.0f / (camera.plane_norm.x * player.dir.y - camera.plane_norm.y * player.dir.x);
    camera.inv_mat.m11 = +player.dir.y * c;
    camera.inv_mat.m12 = -player.dir.x * c;
    camera.inv_mat.m21 = -camera.plane_norm.y * c;
    camera.inv_mat.m22 = +camera.plane_norm.x * c;
  }
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
  for (i = 0; i < FB_LEN; ++i)
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

  for (x = 0; x < FB_WIDTH; ++x) {
    // cast ray
    {
      cam_x = (2.0f * ((f32)x / FB_WIDTH)) - 1.0f;

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

      line_height = FB_HEIGHT / ray_dist;
      if (line_height > FB_HEIGHT)
        line_height = FB_HEIGHT;
      line_y = (FB_HEIGHT - line_height) >> 1;
    } else {
      z_buf[x] = 1e10;
      line_y = FB_HEIGHT >> 1;
    }

    // fill column
    y = 0;
    for (; y < line_y; ++y)
      fb[x + y * FB_WIDTH] = SKY_COLOR;
    if (cell_id) {
      line_y += line_height;
      for (; y < line_y; ++y)
        fb[x + y * FB_WIDTH] = line_color;
    }
    for (; y < FB_HEIGHT; ++y)
      fb[x + y * FB_WIDTH] = FLOOR_COLOR;
  }
}

static void render_sprites(void) {
  vec2f diff, proj;
  u32 w, h;
  u32 i, j, k, n;
  i32 x_start, x_end, y_start, y_end;
  i32 x, y;
  f32 inv_dist_from_player;
  struct sprite* s;
  struct sprite* to_render[MAX_SPRITES_ON_SCREEN];

  n = 0;
  for (i = 0; i < ARRLEN(sprites); ++i) {
    s = sprites + i;

    diff.x = (f32)(s->pos.x - player.pos.x) + (s->dpos.x - player.dpos.x);
    diff.y = (f32)(s->pos.y - player.pos.y) + (s->dpos.y - player.dpos.y);

    // compute coordinates in camera space
    proj.x = camera.inv_mat.m11 * diff.x + camera.inv_mat.m12 * diff.y;
    proj.y = camera.inv_mat.m21 * diff.x + camera.inv_mat.m22 * diff.y;

    // sprite is behind the camera, ignore it
    if (proj.y < 0.0f)
      continue;

    // sprite is not on screen, ignore it
    s->screen_x = FB_WIDTH * (0.5f + proj.x / proj.y);
    if ((u32)s->screen_x >= FB_WIDTH)
      continue;

    // compute distance from player
    s->dist_from_player2 = diff.x * diff.x + diff.y * diff.y;

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

    inv_dist_from_player = inv_sqrt(s->dist_from_player2);

    w = (f32)s->dim.x * inv_dist_from_player;
    h = (f32)s->dim.y * inv_dist_from_player;

    // determine screen coordinates of the sprite
    x_start = s->screen_x - (w >> 1);
    x_end = x_start + w;
    y_end = (f32)(FB_HEIGHT >> 1) * (1.0f + inv_dist_from_player);
    y_start = y_end - h;
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

static void compute_player_position(f32 space, i32 long_dir, i32 side_dir, vec2i* pos, vec2f* dpos) {
  dpos->x += player.dir.x * long_dir * space;
  dpos->y += player.dir.y * long_dir * space;

  dpos->x += -player.dir.y * side_dir * space;
  dpos->y += +player.dir.x * side_dir * space;

  for (; dpos->x >= 1.0f; dpos->x -= 1.0f)
    ++pos->x;
  for (; dpos->x < 0.0f; dpos->x += 1.0f)
    --pos->x;

  for (; dpos->y >= 1.0f; dpos->y -= 1.0f)
    ++pos->y;
  for (; dpos->y < 0.0f; dpos->y += 1.0f)
    --pos->y;
}

// TODO improve player collisions
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

  // compute collision check position
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
  if (!locked)
    keys = (struct keys) {0};
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

void init(void) {
  register_fb(fb, FB_WIDTH, FB_HEIGHT, FB_SIZE);
  clear_screen(0xFFFF0000);
  set_camera_fov(DEG2RAD(90.0f));
  set_player_rot(0);
  camera.dof = 32;
}

void tick(f32 delta) {
  update(delta);
  render();
}
