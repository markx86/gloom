#include <gloom.h>
#include <globals.h>
#include <ui.h>

#include <sprites.c>

// reduced DOF for computing collision rays
#define COLL_DOF 8

#define CROSSHAIR_SIZE      16
#define CROSSHAIR_THICKNESS 2

#define BULLET_SCREEN_OFF 128
#define PLAYER_ANIM_FPS 6

#define BULLET_SPRITE_W (BULLET_TEXTURE_W * 2)
#define BULLET_SPRITE_H (BULLET_TEXTURE_H * 2)

#define PLAYER_SPRITE_W (PLAYER_TILE_W * 5)
#define PLAYER_SPRITE_H (PLAYER_TILE_H * 5)

#define HEALTH_BAR_WIDTH 64
#define HEALTH_BAR_LAG   0.85f

#define PLAYER_POS_INTERP 0.66f

struct player player;
struct map map;
struct sprites sprites;
struct camera camera;
union keys keys;

static f32 z_buf[FB_WIDTH];
static i32 display_health;

static const vec2i sprite_dims[] = {
  [SPRITE_PLAYER] = { .x = PLAYER_SPRITE_W, .y = PLAYER_SPRITE_H },
  [SPRITE_BULLET] = { .x = BULLET_SPRITE_W, .y = BULLET_SPRITE_H }
};

const f32 sprite_radius[] = {
  [SPRITE_PLAYER] = 0.15f,
  [SPRITE_BULLET] = 0.01f
};

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

// use the DDA algorithm to trace a ray
static u8 trace_ray(const vec2f* pos, const vec2f* ray_dir,
                    u32 dof, struct hit* hit) {
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

  hit->dist = vertical ?
              intersec_dist.y - delta_dist.y : intersec_dist.x - delta_dist.x;
  hit->vertical = vertical;

  return cell_id;
}

b8 move_and_collide(vec2f* pos, vec2f* diff, f32 radius) {
  struct hit hit;
  f32 v_dist, h_dist;
  vec2f v_dir, h_dir;
  b8 collided = false;

  // check for collisions on the y-axis
  v_dir.x = 0.0f;
  v_dir.y = signf(diff->y);
  v_dist = absf(diff->y);
  if (trace_ray(pos, &v_dir, COLL_DOF, &hit) &&
      hit.dist < v_dist + radius) {
    v_dist = hit.dist - radius;
    collided = true;
  }

  // check for collisions on the x-axis
  h_dir.x = signf(diff->x);
  h_dir.y = 0.0f;
  h_dist = absf(diff->x);
  if (trace_ray(pos, &h_dir, COLL_DOF, &hit) &&
      hit.dist < h_dist + radius) {
    h_dist = hit.dist - radius;
    collided = true;
  }

  pos->x += h_dir.x * h_dist;
  pos->y += v_dir.y * v_dist;

  return collided;
}

vec2f get_direction_from_keys(void) {
  i32 long_dir, side_dir;
  vec2f dir = {
    .x = 0.0f, .y = 0.0f
  };

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
    // have a length of 1, therefore
    // ||player.long_dir + player.side_dir|| = sqrt(2).
    if (long_dir) {
      dir.x *= INV_SQRT2;
      dir.y *= INV_SQRT2;
    }
  }

  return dir;
}

static inline void update_player_position(f32 delta) {
  vec2f dir;

  if (map.tiles == NULL)
    return;

  // a dead man cannot move :^)
  if (player.health <= 0)
    return;

  dir = get_direction_from_keys();

  move_and_collide(&player.pos,
                   &VEC2SCALE(&dir, delta * PLAYER_RUN_SPEED),
                   sprite_radius[SPRITE_PLAYER]);
}

static inline void update_sprites(f32 delta) {
  b8 collided;
  u32 i, j;
  f32 dist_from_other2, min_dist2;
  f32 s_radius;
  vec2f diff;
  struct sprite *s, *other;

  for (i = 0; i < sprites.n; ++i) {
    s = sprites.s + i;

    s_radius = sprite_radius[s->desc.type];

    collided = move_and_collide(&s->pos,
                                &VEC2SCALE(&s->vel, delta),
                                s_radius);
    // disable bullet sprites on collision with a wall
    if (s->desc.type == SPRITE_BULLET)
      s->disabled = !s->disabled && collided;
    else /* s->desc.type == SPRITE_PLAYER */ {
      // do player sprite animation
      if (s->anim_frame > 4.0f) {
        // if anim_frame > 4, the firing frame is being shown
        s->anim_frame -= delta * PLAYER_ANIM_FPS;
        if (s->anim_frame < 4.0f)
          s->anim_frame = 4.0f;
      }
      else if (VEC2LENGTH2(&s->vel) > 0.01f)
        // player is moving, animate
        s->anim_frame = modf(s->anim_frame + delta * PLAYER_ANIM_FPS, 4.0f);
      else
        // player is standing, set the correct animation frame
        s->anim_frame = 4.0f;
    }

    // if the sprite is disabled or the sprite is not a player,
    // do not do collision checks with other sprites.
    if (s->disabled || s->desc.type != SPRITE_PLAYER)
      continue;

    for (j = 0; j < sprites.n; ++j) {
      other = sprites.s + j;

      if (other->disabled || other == s)
        continue;

      // since we already need to compute dist_from_player2, might as well
      // save the diff vector, because we'll also need it during rendering.
      diff = VEC2SUB(&other->pos, &s->pos);

      // compute minimum distance between the two sprites
      min_dist2 = s_radius + sprite_radius[other->desc.id];
      min_dist2 *= min_dist2;

      // compute the distance from the player and check if the sprite collided
      dist_from_other2 = VEC2LENGTH2(&diff);
      if (dist_from_other2 < min_dist2) {
        if (other->desc.type == SPRITE_BULLET &&
            other->desc.owner != s->desc.id)
          // disable the bullet sprite if it collided with a player sprite.
          other->disabled = true;
      }
    }
  }

  UNUSED(delta);
}

void gloom_update(f32 delta) {
  update_player_position(delta);
  update_sprites(delta);
}

static void draw_column(u8 cell_id, i32 x, const struct hit* hit) {
  i32 y, line_y, line_height;
  u32 line_color;

  // draw column
  if (cell_id) {
    line_color = hit->vertical ? COLOR(WHITE) : COLOR(LIGHTGRAY);

    line_height = FB_HEIGHT / hit->dist;
    if ((u32)line_height > FB_HEIGHT)
      line_height = FB_HEIGHT;
    line_y = (FB_HEIGHT - line_height) >> 1;
  } else
    line_y = FB_HEIGHT >> 1;

  // fill column
  y = 0;
  for (; y < line_y; ++y)
    set_pixel(x, y, COLOR(BLUE));
  if (cell_id) {
    line_y += line_height;
    for (; y < line_y; ++y)
      set_pixel(x, y, line_color);
  }
  for (; y < FB_HEIGHT; ++y)
    set_pixel(x, y, COLOR(BLACK));
}

static inline i32 get_y_end(struct sprite* s, u32 screen_h) {
  switch (s->desc.type) {
    case SPRITE_BULLET:
      // we add a little offset to the bullet's vertical height so
      // that it doesn't come out of the player camera
      return (FB_HEIGHT + screen_h +
              (i32)((f32)BULLET_SCREEN_OFF * s->inv_depth)) >> 1;
    default:
      // by default, place objects on the ground
      return (f32)(FB_HEIGHT >> 1) * (1.0f + s->inv_depth);
  }
}

static inline const u8* get_player_tile(u32 x, u32 y) {
  return &player_spritesheet[((y * PLAYER_NTILES_W) + x) *
                             (PLAYER_TILE_W * PLAYER_TILE_H)];
}

static void get_texture_info(struct sprite* s, b8* invert_x,
                                        u32* tex_w, u32* tex_h,
                                        const u8** tex, const u32** coltab) {
  i32 rot;

  *invert_x = false;

  // do sprite specific stuff
  if (s->desc.type == SPRITE_PLAYER) {
    // set the player sprite data
#define STEPS ((PLAYER_NTILES_H << 1) - 2)
#define SLICE (TWO_PI / STEPS)
    // determine the rotation of the sprite to use
    // FIXME: this doesn't look right in practice, maybe take a look at this?
    rot = (s->rot + SLICE / 2.0f - player.rot + PI) * STEPS / TWO_PI;
    rot &= 7;
    if (rot > 4) {
      rot = 8 - rot;
      *invert_x = true;
    }
    // get the pointer to the corresponding sprite texture
    *tex = get_player_tile((u32)s->anim_frame, abs(rot));
    // set the color table pointer
    *coltab = player_coltab;
    // set the texture width and height
    *tex_w = PLAYER_TILE_W;
    *tex_h = PLAYER_TILE_H;
  } else /* s->desc.type == SPRITE_BULLET */ {
    // set the bullet sprite data
    *tex = bullet_texture;
    *coltab = bullet_coltab;
    // set bullet texture dimensions
    *tex_w = BULLET_TEXTURE_W;
    *tex_h = BULLET_TEXTURE_H;
  }
}

static void draw_sprite(struct sprite* s) {
  u32 screen_h, color, a;
  u32 tex_w, tex_h;
  u32 uvw, uvh, uvx, uvy;
  i32 x_start, x_end, y_start, y_end;
  i32 x, y;
  const u8* tex;
  const u32* coltab;
  b8 invert_x = false;

  screen_h = (f32)sprite_dims[s->desc.type].y * s->inv_depth;

  // determine screen coordinates of the sprite
  x_start = s->screen_x - s->screen_halfw;
  x_end = s->screen_x + s->screen_halfw;
  y_end = get_y_end(s, screen_h);
  y_start = y_end - screen_h;

  // compute the sprite width and height on the screen
  uvw = MAX(x_end - x_start, 0);
  uvh = MAX(y_end - y_start, 0);

  get_texture_info(s, &invert_x, &tex_w, &tex_h, &tex, &coltab);

  a = get_alpha_mask();
  // draw the sprite
  for (x = MAX(0, x_start); x < x_end && x < FB_WIDTH; x++) {
    // discard stripe if there's a wall closer to the camera
    if (z_buf[x] < s->depth2)
      continue;
    // compute x texture coordinate
    uvx = (f32)((x - x_start) * tex_w) / uvw;
    // invert on the x axis if needed
    if (invert_x)
      uvx = tex_w - uvx;
    for (y = MAX(0, y_start); y < y_end && y < FB_HEIGHT; y++) {
      uvy = (f32)((y - y_start) * tex_h) / uvh;
      color = tex[uvx + uvy * tex_w];
      if (color)
        set_pixel(x, y, coltab[color] | a);
    }
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

  // interpolate camera position with real position
  player.fake_pos.x = player.pos.x * PLAYER_POS_INTERP +
                      player.fake_pos.x * (1.0f - PLAYER_POS_INTERP);
  player.fake_pos.y = player.pos.y * PLAYER_POS_INTERP +
                      player.fake_pos.y * (1.0f - PLAYER_POS_INTERP);

  for (x = 0; x < FB_WIDTH; ++x) {
    // compute ray direction
    cam_x = (2.0f * ((f32)x / FB_WIDTH)) - 1.0f;
    ray_dir.x = player.dir.x + camera.plane.x * cam_x;
    ray_dir.y = player.dir.y + camera.plane.y * cam_x;
    // trace ray with DDA
    cell_id = trace_ray(&player.fake_pos, &ray_dir, camera.dof, &hit);
    // store distance (squared) in z-buffer
    z_buf[x] = hit.dist * hit.dist;

    draw_column(cell_id, x, &hit);
  }
}

static inline void render_sprites(void) {
  vec2f proj, diff;
  u32 i, j, k, n;
  struct sprite *s, *on_screen_sprites[MAX_SPRITES];

  n = 0;
  for (i = 0; i < sprites.n; ++i) {
    s = sprites.s + i;

    // do not render disabled sprites
    if (s->disabled)
      continue;

    // do not render the tracked sprite
    // FIXME: find a better way to do this maybe?
    if (s == tracked_sprite)
      continue;

    diff = VEC2SUB(&s->pos, &player.fake_pos);

    // compute coordinates in camera space
    proj.x = camera.inv_mat.m11 * diff.x + camera.inv_mat.m12 * diff.y;
    proj.y = camera.inv_mat.m21 * diff.x + camera.inv_mat.m22 * diff.y;

    // sprite is behind the camera, ignore it
    if (proj.y < 0.0f)
      continue;

    // save camera depth
    s->inv_depth = 1.0f / proj.y;

    // compute screen x
    s->screen_x = (FB_WIDTH >> 1) * (1.0f + proj.x / proj.y);
    // compute screen width (we divide by two since
    // we always use the half screen width)
    s->screen_halfw =
      (i32)((f32)sprite_dims[s->desc.type].x * s->inv_depth) >> 1;

    // sprite is not on screen, ignore it
    if (s->screen_x + s->screen_halfw < 0 ||
        s->screen_x - s->screen_halfw >= FB_WIDTH)
      continue;

    s->depth2 = proj.y * proj.y;

    // look for index to insert the new sprite
    for (j = 0; j < n; ++j) {
      if (on_screen_sprites[j]->depth2 < s->depth2)
        break;
    }
    // move elements after the entry to the next index
    for (k = n; k > j; --k)
      on_screen_sprites[k] = on_screen_sprites[k-1];
    // store the new sprite to render
    on_screen_sprites[j] = s;

    if (++n >= MAX_SPRITES)
      break;
  }

  for (i = 0; i < n; i++)
    draw_sprite(on_screen_sprites[i]);
}

static inline u32 invert_color(u32 color) {
  union {
    u32 u;
    struct {
      u32 r : 8;
      u32 g : 8;
      u32 b : 8;
      u32 a : 8;
    };
  } comps;

  comps.u = color;

  comps.r = 0xFF - comps.r;
  comps.g = 0xFF - comps.g;
  comps.b = 0xFF - comps.b;
  comps.a = 0;

  return comps.u | get_alpha_mask();
}

static inline void render_crosshair(void) {
  u32 i, j;
  u32 x, y;
  u32 px;
  b8 draw_x, draw_y;
  vec2u coords = {
    .x = (FB_WIDTH  - CROSSHAIR_SIZE) >> 1,
    .y = (FB_HEIGHT - CROSSHAIR_SIZE) >> 1,
  };

#define CROSSHAIR_DRAW_START ((CROSSHAIR_SIZE - CROSSHAIR_THICKNESS) >> 1)
#define CROSSHAIR_DRAW_END   ((CROSSHAIR_SIZE + CROSSHAIR_THICKNESS) >> 1)

  for (i = 0; i < CROSSHAIR_SIZE; ++i) {
    draw_x = i >= CROSSHAIR_DRAW_START && i < CROSSHAIR_DRAW_END;
    for (j = 0; j < CROSSHAIR_SIZE; ++j) {
      draw_y = j >= CROSSHAIR_DRAW_START && j < CROSSHAIR_DRAW_END;
      if (draw_x || draw_y) {
        x = coords.x + i;
        y = coords.y + j;
        px = get_pixel(x, y);
        px = invert_color(px);
        set_pixel(x, y, px);
      }
    }
  }
}

static inline void render_hud(void) {
  u32 health_bar_w, health_bar_c, x;
  b8 got_damage;
  const char health_lbl[] = "H";

  // check if the player received damage
  got_damage = display_health != player.health;

  x = 8 + STRING_WIDTH_IMM(health_lbl) + 4;
  if (got_damage)
    // if the player received damage, draw a rectangle around the health
    // bar to draw the attention of the player
    draw_rect(4, 4, x + HEALTH_BAR_WIDTH, 8 + STRING_HEIGHT, COLOR(MAGENTA));

  health_bar_c = COLOR(RED);
  draw_string_with_color(8, 8, health_lbl, health_bar_c);

  health_bar_w = (f32)(player.health * HEALTH_BAR_WIDTH) / PLAYER_MAX_HEALTH;
  draw_rect(x, 8, health_bar_w, STRING_HEIGHT - 1, health_bar_c);

  if (got_damage) {
    // if the player received damage, animate the health difference
    x += health_bar_w;
    health_bar_w = (f32)((display_health - player.health) * HEALTH_BAR_WIDTH)
                    / PLAYER_MAX_HEALTH;
    draw_rect(x, 8, health_bar_w, STRING_HEIGHT - 1, COLOR(WHITE));

    display_health = (display_health * HEALTH_BAR_LAG +
                      player.health * (1.0f - HEALTH_BAR_LAG));
  }
}

void gloom_render(void) {
  render_scene();
  render_sprites();
  render_crosshair();
  render_hud();
}

void gloom_init(void) {
  // may seem counter intuitive, you should look into
  // the hello packet handler in multiplayer.c
  set_player_rot(player.rot);
  display_health = player.health = PLAYER_MAX_HEALTH;
}

void gloom_tick(f32 delta) {
  gloom_update(delta);
  gloom_render();
}
