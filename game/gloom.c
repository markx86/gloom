#include <gloom.h>

struct camera camera;
f32 z_buf[FB_WIDTH];

struct player player = {
  .pos = {
    .x = 2,
    .y = 2,
  },
};

struct map map = {
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

struct sprite sprites[] = {
  [0] = {
    .color = 0xFF0000FF,
    .pos = {
      .x = 5,
      .y = 5
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
    .dim = {
      .x = 128,
      .y = 480,
    }
  }
};
union keys keys;

// NOTE @new_fov must be in radians
void set_camera_fov(f32 new_fov) {
  camera.fov = new_fov;
  camera.plane_halfw = 1.0f / (2.0f * tan(new_fov / 2.0f));
}

// NOTE @new_rot must be in radians
void set_player_rot(f32 new_rot) {
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

static u8 trace_ray(const vec2f* ray_dir, struct hit* hit) {
  vec2f delta_dist, dist, intersec_dist;
  vec2f dpos;
  vec2i step_dir;
  vec2u map_coords;
  u32 d;
  b8 vertical, cell_id;

  dpos.x = player.pos.x - (i32)player.pos.x;
  dpos.y = player.pos.y - (i32)player.pos.y;

  delta_dist.y = absf(1.0f / ray_dir->x);
  delta_dist.x = absf(1.0f / ray_dir->y);

  dist.x = isposf(ray_dir->x) ? (1.0f - dpos.x) : dpos.x;
  dist.y = isposf(ray_dir->y) ? (1.0f - dpos.y) : dpos.y;

  intersec_dist.y = delta_dist.y * dist.x;
  intersec_dist.x = delta_dist.x * dist.y;

  step_dir.x = signf(ray_dir->x);
  step_dir.y = signf(ray_dir->y);

  map_coords.x = (u32)player.pos.x;
  map_coords.y = (u32)player.pos.y;

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
  hit->vertical = vertical;

  return cell_id;
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
  v_dir.y = signf(dir.y);
  v_dist = absf(dir.y) * space;
  if (trace_ray(&v_dir, &hit) && hit.dist < v_dist + PLAYER_RADIUS)
    v_dist = hit.dist - PLAYER_RADIUS;

  // check for collisions on the x-axis
  h_dir.x = signf(dir.x);
  h_dir.y = 0.0f;
  h_dist = absf(dir.x) * space;
  if (trace_ray(&h_dir, &hit) && hit.dist < h_dist + PLAYER_RADIUS)
    h_dist = hit.dist - PLAYER_RADIUS;

  player.pos.x += h_dir.x * h_dist;
  player.pos.y += v_dir.y * v_dist;
}

static inline void update_sprites(f32 delta) {
  u32 i;
  struct sprite* s;

  for (i = 0; i < ARRLEN(sprites); ++i) {
    s = sprites + i;

    // since we already need to compute dist_from_player2, might as well
    // save the diff vector, because we'll also need it during rendering.
    s->diff.x = s->pos.x - player.pos.x;
    s->diff.y = s->pos.y - player.pos.y;
    // we need to compute dist_from_player2 here, because we need to use it
    // for collision detection with the player.
    s->dist_from_player2 = s->diff.x * s->diff.x + s->diff.y * s->diff.y;

    if (s->dist_from_player2 < PLAYER_RADIUS * PLAYER_RADIUS) {
      // sprite collided with the player
    }
  }

  UNUSED(delta);
}

static inline void update(f32 delta) {
  update_player_position(delta);
  update_sprites(delta);
}

static inline void render_scene(void) {
  u8 cell_id;
  i32 x;
  f32 cam_x;
  vec2f ray_dir;
  struct hit hit;

  for (x = 0; x < FB_WIDTH; ++x) {
    cam_x = (2.0f * ((f32)x / FB_WIDTH)) - 1.0f;

    // we do not use VEC2* macros, because this is faster
    ray_dir.x = player.long_dir.x + camera.plane.x * cam_x;
    ray_dir.y = player.long_dir.y + camera.plane.y * cam_x;

    cell_id = trace_ray(&ray_dir, &hit);

    z_buf[x] = hit.dist * hit.dist;

    draw_column(cell_id, x, &hit);
  }
}

static inline void render_sprites(void) {
  vec2f proj;
  u32 i, j, k, n;
  struct sprite* s;
  struct sprite* on_screen_sprites[MAX_SPRITES_ON_SCREEN];

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
      if (on_screen_sprites[j]->dist_from_player2 < s->dist_from_player2)
        break;
    }
    // move elements after the entry to the next index
    for (k = n; k > j; --k)
      on_screen_sprites[k] = on_screen_sprites[k-1];
    // store the new sprite to render
    on_screen_sprites[j] = s;

    if (++n >= MAX_SPRITES_ON_SCREEN)
      break;
  }

  for (i = 0; i < n; i++)
    draw_sprite(on_screen_sprites[i]);
}

static inline void render(void) {
  render_scene();
  render_sprites();
}

void game_tick(f32 delta) {
  update(delta);
  render();
}
