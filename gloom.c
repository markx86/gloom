#include <types.h>
#include <env.h>
#include <libc.h>
#include <math.h>

#define FB_WIDTH  640
#define FB_HEIGHT 480
#define FB_SIZE   sizeof(fb)
#define FB_LEN    ARRLEN(fb)

#define COLOR_SKY   0xFFFF0000
#define COLOR_FLOOR 0xFF000000
#define COLOR_WALLH 0xFFFFFFFF
#define COLOR_WALLV 0xFFAAAAAA

#define PLAYER_RUN_SPEED    2.0f
#define PLAYER_ROT_SPEED    0.01f
#define PLAYER_RADIUS       0.15f

#define CAMERA_FOV 75.0f
#define CAMERA_DOF 32

#define MAX_SPRITES_ON_SCREEN 128

struct camera {
  u32 dof;
  f32 fov;
  f32 plane_halfw;
  vec2f plane;
  struct {
    f32 m11, m12, m21, m22;
  } inv_mat;
};

struct actor {
  vec2i pos;
  vec2f dpos;
  f32 rot;
  vec2f long_dir;
  vec2f side_dir;
};

struct sprite {
  u32 color;
  vec2i pos;
  vec2f dpos;
  vec2i dim;
  struct {
    i32 screen_x;
    i32 screen_halfw;
    f32 camera_depth;
    f32 dist_from_player2;
    vec2f diff;
  };
};

struct map {
  u32 w, h;
  u8 tiles[];
};

struct keys {
  b8 forward;
  b8 backward;
  b8 right;
  b8 left;
};

struct hit {
  f32 dist;
  b8 vertical;
  u8 cell_id;
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
  .tiles = {
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
  player.long_dir.x = cos(new_rot);
  player.long_dir.y = sin(new_rot);
  // compute camera plane vector
  player.side_dir.x = -player.long_dir.y;
  player.side_dir.y = +player.long_dir.x;
  // compute camera plane offset
  camera.plane = VEC2SCALE(player.side_dir, camera.plane_halfw);
  // compute camera inverse matrix
  {
    c = 1.0f / (camera.plane.x * player.long_dir.y - camera.plane.y * player.long_dir.x);
    camera.inv_mat.m11 = +player.long_dir.y * c;
    camera.inv_mat.m12 = -player.long_dir.x * c;
    camera.inv_mat.m21 = -camera.plane.y * c;
    camera.inv_mat.m22 = +camera.plane.x * c;
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

static inline void clear_screen(u32 color) {
  u32 i;
  for (i = 0; i < FB_LEN; ++i)
    fb[i] = color;
}

static void trace_ray(const vec2f* ray_dir, struct hit* hit) {
  vec2f delta_dist, dist, intersec_dist;
  vec2i step_dir;
  vec2u map_coords;
  u32 d;
  b8 vertical, cell_id;

  delta_dist.y = abs(1.0f / ray_dir->x);
  delta_dist.x = abs(1.0f / ray_dir->y);

  dist.x = ray_dir->x > 0 ? (1.0f - player.dpos.x) : player.dpos.x;
  dist.y = ray_dir->y > 0 ? (1.0f - player.dpos.y) : player.dpos.y;

  intersec_dist.y = delta_dist.y * dist.x;
  intersec_dist.x = delta_dist.x * dist.y;

  step_dir.x = SIGN(ray_dir->x);
  step_dir.y = SIGN(ray_dir->y);

  map_coords = REINTERPRET(player.pos, vec2u);

  vertical = true;
  cell_id = 0;
  for (d = 0; d < camera.dof; ++d) {
    if (map_coords.x >= map.w || map_coords.y >= map.h)
      break;

    if ((cell_id = map.tiles[map_coords.x + map_coords.y * map.w]))
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

  hit->dist = vertical ? intersec_dist.y - delta_dist.y : intersec_dist.x - delta_dist.x;
  hit->cell_id = cell_id;
  hit->vertical = vertical;
}

static inline void render_scene(void) {
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

    trace_ray(&ray_dir, &hit);

    // draw column
    if (hit.cell_id) {
      line_color = hit.vertical ? COLOR_WALLV : COLOR_WALLH;

      line_height = FB_HEIGHT / hit.dist;
      if (line_height > FB_HEIGHT)
        line_height = FB_HEIGHT;
      line_y = (FB_HEIGHT - line_height) >> 1;
    } else
      line_y = FB_HEIGHT >> 1;

    z_buf[x] = hit.dist * hit.dist;

    // fill column
    y = 0;
    for (; y < line_y; ++y)
      fb[x + y * FB_WIDTH] = COLOR_SKY;
    if (hit.cell_id) {
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

static void adjust_position(vec2i* pos, vec2f* dpos) {
  for (; dpos->x >= 1.0f; dpos->x -= 1.0f)
    ++pos->x;
  for (; dpos->x < 0.0f; dpos->x += 1.0f)
    --pos->x;

  for (; dpos->y >= 1.0f; dpos->y -= 1.0f)
    ++pos->y;
  for (; dpos->y < 0.0f; dpos->y += 1.0f)
    --pos->y;
}

static inline void update_player_position(f32 delta) {
  struct hit hit;
  i32 long_dir, side_dir;
  f32 space;
  f32 v_dist, h_dist;
  vec2f v_dir, h_dir;
  vec2f dir = {
    .x = 0.0f, .y = 0.0f
  };

  long_dir = keys.forward - keys.backward;
  side_dir = keys.right - keys.left;

  space = delta * PLAYER_RUN_SPEED;

  // forward movement
  if (long_dir) {
    dir.x += player.long_dir.x * long_dir;
    dir.y += player.long_dir.y * long_dir;
  }
  // sideways movement
  if (side_dir) {
    dir.x += player.side_dir.x * side_dir;
    dir.y += player.side_dir.y * side_dir;

    // if the user is trying to go both forwards and sideways,
    // we normalize the vector by dividing by sqrt(2).
    // we divide by sqrt(2), because both player.long_dir and player.side_dir,
    // have a length of 1, therefore ||player.long_dir + player.side_dir|| = sqrt(2).
    if (long_dir) {
      dir.x *= INV_SQRT2;
      dir.y *= INV_SQRT2;
    }
  }

  // check for collisions on the y-axis
  v_dir.x = 0.0f;
  v_dir.y = SIGN(dir.y);
  v_dist = absf(dir.y) * space;
  trace_ray(&v_dir, &hit);
  if (hit.dist < v_dist + PLAYER_RADIUS)
    v_dist = hit.dist - PLAYER_RADIUS;

  // check for collisions on the x-axis
  h_dir.x = SIGN(dir.x);
  h_dir.y = 0.0f;
  h_dist = absf(dir.x) * space;
  trace_ray(&h_dir, &hit);
  if (hit.dist < h_dist + PLAYER_RADIUS)
    h_dist = hit.dist - PLAYER_RADIUS;

  player.dpos.x += h_dir.x * h_dist;
  player.dpos.y += v_dir.y * v_dist;
  adjust_position(&player.pos, &player.dpos);
}

static inline void update_sprites(f32 delta) {
  u32 i;
  struct sprite* s;

  for (i = 0; i < ARRLEN(sprites); ++i) {
    s = sprites + i;

    // since we already need to compute dist_from_player2, might as well
    // save the diff vector, because we'll also need it during rendering.
    s->diff.x = (f32)(s->pos.x - player.pos.x) + (s->dpos.x - player.dpos.x);
    s->diff.y = (f32)(s->pos.y - player.pos.y) + (s->dpos.y - player.dpos.y);
    // we need to compute dist_from_player2 here, because we need to use it
    // for collision detection with the player.
    s->dist_from_player2 = s->diff.x * s->diff.x + s->diff.y * s->diff.y;
  }

  UNUSED(delta);
}

static void update(f32 delta) {
  update_player_position(delta);
  update_sprites(delta);
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
  off_player_rot(dx * PLAYER_ROT_SPEED);

  UNUSED(x);
  UNUSED(y);
  UNUSED(dy);
}

void init(void) {
  register_fb(fb, FB_WIDTH, FB_HEIGHT, FB_SIZE);
  clear_screen(0xFFFF0000);
  set_camera_fov(DEG2RAD(CAMERA_FOV));
  set_player_rot(0);
  camera.dof = CAMERA_DOF;
}

void tick(f32 delta) {
  update(delta);
  render();
}
