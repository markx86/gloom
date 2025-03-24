#include <client.h>

#define COLOR_SKY   0xFF0000
#define COLOR_FLOOR 0x000000
#define COLOR_WALLH 0xFFFFFF
#define COLOR_WALLV 0xAAAAAA

struct colors {
  u32 sky;
  u32 floor;
  u32 wall_h;
  u32 wall_v;
};

// reduced DOF for computing collision rays
#define COLL_DOF 8

struct player player;
struct map map;
union keys keys;
struct sprites sprites;
struct camera camera;

static struct colors colors;
static u32 alpha;

f32 z_buf[FB_WIDTH];

void set_alpha(u8 a) {
  alpha = ((u32)a) << 24;
  colors.sky    = alpha | COLOR_SKY;
  colors.floor  = alpha | COLOR_FLOOR;
  colors.wall_h = alpha | COLOR_WALLH;
  colors.wall_v = alpha | COLOR_WALLV;
}

// NOTE: @new_fov must be in radians
void set_camera_fov(f32 new_fov) {
  camera.fov = new_fov;
  camera.plane_halfw = 1.0f / (2.0f * tan(new_fov / 2.0f));
}

// NOTE: @new_rot must be in radians
void set_player_rot(f32 new_rot) {
  f32 c;

  player.rot = new_rot;
  // compute player versor
  player.dir.x = cos(new_rot);
  player.dir.y = sin(new_rot);
  // compute camera plane offset
  camera.plane.x = -player.dir.y * camera.plane_halfw;
  camera.plane.y = +player.dir.x * camera.plane_halfw;
  // compute camera inverse matrix
  {
    c = 1.0f / (camera.plane.x * player.dir.y - camera.plane.y * player.dir.x);
    camera.inv_mat.m11 = +player.dir.y * c;
    camera.inv_mat.m12 = -player.dir.x * c;
    camera.inv_mat.m21 = -camera.plane.y * c;
    camera.inv_mat.m22 = +camera.plane.x * c;
  }
}

static u8 trace_ray(const vec2f* pos, const vec2f* ray_dir, u32 dof, struct hit* hit) {
  vec2f delta_dist, dist, intersec_dist;
  vec2f dpos;
  vec2i step_dir;
  vec2u map_coords;
  u32 d;
  b8 vertical, cell_id;

  dpos.x = pos->x - (i32)pos->x;
  dpos.y = pos->y - (i32)pos->y;

  delta_dist.y = absf(1.0f / ray_dir->x);
  delta_dist.x = absf(1.0f / ray_dir->y);

  dist.x = isposf(ray_dir->x) ? (1.0f - dpos.x) : dpos.x;
  dist.y = isposf(ray_dir->y) ? (1.0f - dpos.y) : dpos.y;

  intersec_dist.y = delta_dist.y * dist.x;
  intersec_dist.x = delta_dist.x * dist.y;

  step_dir.x = signf(ray_dir->x);
  step_dir.y = signf(ray_dir->y);

  map_coords.x = (u32)pos->x;
  map_coords.y = (u32)pos->y;

  vertical = true;
  cell_id = 0;
  for (d = 0; d < dof; ++d) {
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

static void move_and_collide(vec2f* pos, vec2f* dir, f32 space) {
  struct hit hit;
  f32 v_dist, h_dist;
  vec2f v_dir, h_dir;

  // check for collisions on the y-axis
  v_dir.x = 0.0f;
  v_dir.y = signf(dir->y);
  v_dist = absf(dir->y) * space;
  if (trace_ray(pos, &v_dir, COLL_DOF, &hit) && hit.dist < v_dist + SPRITE_RADIUS)
    v_dist = hit.dist - SPRITE_RADIUS;

  // check for collisions on the x-axis
  h_dir.x = signf(dir->x);
  h_dir.y = 0.0f;
  h_dist = absf(dir->x) * space;
  if (trace_ray(pos, &h_dir, COLL_DOF, &hit) && hit.dist < h_dist + SPRITE_RADIUS)
    h_dist = hit.dist - SPRITE_RADIUS;

  pos->x += h_dir.x * h_dist;
  pos->y += v_dir.y * v_dist;
}

static inline void update_player_position(f32 delta) {
  i32 long_dir, side_dir;
  vec2f dir = {
    .x = 0.0f, .y = 0.0f
  };

  if (map.tiles == NULL)
    return;

  long_dir = keys.forward - keys.backward;
  side_dir = keys.right - keys.left;

  // forward movement
  if (long_dir) {
    dir.x += player.dir.x * long_dir;
    dir.y += player.dir.y * long_dir;
  }
  // sideways movement
  if (side_dir) {
    dir.x += -player.dir.y * side_dir;
    dir.y += +player.dir.x * side_dir;

    // if the user is trying to go both forwards and sideways,
    // we normalize the vector by dividing by sqrt(2).
    // we divide by sqrt(2), because both player.long_dir and player.side_dir,
    // have a length of 1, therefore ||player.long_dir + player.side_dir|| = sqrt(2).
    if (long_dir) {
      dir.x *= INV_SQRT2;
      dir.y *= INV_SQRT2;
    }
  }

  move_and_collide(&player.pos, &dir, delta * PLAYER_RUN_SPEED);
}

static inline void update_sprites(f32 delta) {
  u32 i;
  struct sprite* s;

  for (i = 0; i < sprites.n; ++i) {
    s = sprites.s + i;

    move_and_collide(&s->pos, &s->dir, s->vel * delta);

    // since we already need to compute dist_from_player2, might as well
    // save the diff vector, because we'll also need it during rendering.
    s->diff.x = s->pos.x - player.pos.x;
    s->diff.y = s->pos.y - player.pos.y;
    // we need to compute dist_from_player2 here, because we need to use it
    // for collision detection with the player.
    s->dist_from_player2 = s->diff.x * s->diff.x + s->diff.y * s->diff.y;

    if (s->dist_from_player2 < SPRITE_RADIUS * SPRITE_RADIUS) {
      // sprite collided with the player
    }
  }

  UNUSED(delta);
}

static inline void update(f32 delta) {
  update_player_position(delta);
  update_sprites(delta);
}

static void draw_column(u8 cell_id, i32 x, const struct hit* hit) {
  i32 y, line_y, line_height;
  u32 line_color;

  // draw column
  if (cell_id) {
    line_color = hit->vertical ? colors.wall_v : colors.wall_h;

    line_height = FB_HEIGHT / hit->dist;
    if ((u32)line_height > FB_HEIGHT)
      line_height = FB_HEIGHT;
    line_y = (FB_HEIGHT - line_height) >> 1;
  } else
    line_y = FB_HEIGHT >> 1;

  // fill column
  y = 0;
  for (; y < line_y; ++y)
    fb[x + y * FB_WIDTH] = colors.sky;
  if (cell_id) {
    line_y += line_height;
    for (; y < line_y; ++y)
      fb[x + y * FB_WIDTH] = line_color;
  }
  for (; y < FB_HEIGHT; ++y)
    fb[x + y * FB_WIDTH] = colors.floor;
}

static void draw_sprite(struct sprite* s) {
  u32 screen_h, color;
  i32 x_start, x_end, y_start, y_end;
  i32 x, y;

  screen_h = (f32)s->dim.y * s->camera_depth;
  color = alpha | s->ic.color;

  // determine screen coordinates of the sprite
  x_start = s->screen_x - s->screen_halfw;
  x_end = s->screen_x + s->screen_halfw;
  y_end = (f32)(FB_HEIGHT >> 1) * (1.0f + s->camera_depth);
  y_start = y_end - screen_h;
  // draw the sprite
  for (x = MAX(0, x_start); x < x_end && x < FB_WIDTH; x++) {
    // FIXME: sprites get clipped the further away they are from the player,
    //        even though they're not behind walls
    if (z_buf[x] < s->dist_from_player2)
      continue;
    for (y = MAX(0, y_start); y < y_end && y < FB_HEIGHT; y++)
      fb[x + y * FB_WIDTH] = color;
  }
}

static inline void render_scene(void) {
  u8 cell_id;
  i32 x;
  f32 cam_x;
  vec2f ray_dir;
  struct hit hit;

  if (map.tiles == NULL)
    return;

  for (x = 0; x < FB_WIDTH; ++x) {
    cam_x = (2.0f * ((f32)x / FB_WIDTH)) - 1.0f;

    // we do not use VEC2* macros, because this is faster
    ray_dir.x = player.dir.x + camera.plane.x * cam_x;
    ray_dir.y = player.dir.y + camera.plane.y * cam_x;

    cell_id = trace_ray(&player.pos, &ray_dir, camera.dof, &hit);

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
  for (i = 0; i < sprites.n; ++i) {
    s = sprites.s + i;

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

void gloom_tick(f32 delta) {
  update(delta);
  render();
}
