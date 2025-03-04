#include <gloom.h>

struct camera camera;

struct player player = {
  .pos = {
    .x = 2,
    .y = 2,
  },
  .dpos = {
    .x = 0.5f,
    .y = 0.5f,
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

f32 z_buf[FB_WIDTH];

struct keys keys;

struct sprite sprites[] = {
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

u8 trace_ray(const vec2f* ray_dir, struct hit* hit) {
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
  hit->vertical = vertical;

  return cell_id;
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
  if (trace_ray(&v_dir, &hit) && hit.dist < v_dist + PLAYER_RADIUS)
    v_dist = hit.dist - PLAYER_RADIUS;

  // check for collisions on the x-axis
  h_dir.x = SIGN(dir.x);
  h_dir.y = 0.0f;
  h_dist = absf(dir.x) * space;
  if (trace_ray(&h_dir, &hit) && hit.dist < h_dist + PLAYER_RADIUS)
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

    if (s->dist_from_player2 < PLAYER_RADIUS * PLAYER_RADIUS) {
      // sprite collided with the player
    }
  }

  UNUSED(delta);
}

void update(f32 delta) {
  update_player_position(delta);
  update_sprites(delta);
}
