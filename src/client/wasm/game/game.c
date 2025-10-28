#include <gloom/game.h>
#include <gloom/ui.h>
#include <gloom/libc.h>
#include <gloom/color.h>
#include <gloom/globals.h>

#include "sprites.c"

/* Reduced DOF for computing collision rays */
#define COLL_DOF 8

#define CROSSHAIR_SIZE      16
#define CROSSHAIR_THICKNESS 2

#define BULLET_SCREEN_OFF 128
#define PLAYER_ANIM_FPS 6

#define BULLET_SPRITE_W (BULLET_TEXTURE_W * 2)
#define BULLET_SPRITE_H (BULLET_TEXTURE_H * 2)

#define PLAYER_SPRITE_W (PLAYER_TILE_W * 5)
#define PLAYER_SPRITE_H (PLAYER_TILE_H * 5)

#define MINIMAP_TILE_W 4
#define MINIMAP_TILE_H 4

#define HEALTH_BAR_WIDTH 64
#define HEALTH_BAR_LAG   0.85f

#define CAMERA_POS_INTERP 0.66f

static f32 g_zbuf[FB_WIDTH];
static i32 g_display_health;

const vec2i g_sprite_dims[] = {
  [SPRITE_PLAYER] = { .x = PLAYER_SPRITE_W, .y = PLAYER_SPRITE_H },
  [SPRITE_BULLET] = { .x = BULLET_SPRITE_W, .y = BULLET_SPRITE_H }
};

const f32 g_sprite_radius[] = {
  [SPRITE_PLAYER] = 0.20f,
  [SPRITE_BULLET] = 0.01f
};

/* NOTE: @new_fov must be in radians */
void game_camera_set_fov(f32 new_fov) {
  g_camera.fov = new_fov;
  g_camera.plane_halfw = 1.0f / (2.0f * tan(new_fov / 2.0f));
}

/* NOTE: @new_rot must be in radians */
void game_player_set_rot(f32 new_rot) {
  f32 c;

  g_player.rot = new_rot;
  /* Compute player versor */
  g_player.dir.x = cos(new_rot);
  g_player.dir.y = sin(new_rot);
  /* Compute camera plane offset */
  g_camera.plane.x = -g_player.dir.y * g_camera.plane_halfw;
  g_camera.plane.y = +g_player.dir.x * g_camera.plane_halfw;
  /* Compute camera inverse matrix */
  {
    c = g_camera.plane.x * g_player.dir.y - g_camera.plane.y * g_player.dir.x;
    c = 1.0f / c;
    g_camera.inv_mat.m11 = +g_player.dir.y * c;
    g_camera.inv_mat.m12 = -g_player.dir.x * c;
    g_camera.inv_mat.m21 = -g_camera.plane.y * c;
    g_camera.inv_mat.m22 = +g_camera.plane.x * c;
  }
}

/* NOTE: @delta must be in radians */
void game_player_add_rot(f32 delta) {
  f32 new_rot = g_player.rot + delta;
  if (new_rot >= TWO_PI)
    new_rot -= TWO_PI;
  else if (new_rot < 0.0f)
    new_rot += TWO_PI;
  game_player_set_rot(new_rot);
}

/* Use the DDA algorithm to trace a ray */
static
u8 trace_ray(const vec2f* pos, const vec2f* ray_dir, u32 dof, struct hit* hit) {
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
    if (map_coords.x >= g_map.w || map_coords.y >= g_map.h)
      break;

    if ((cell_id = g_map.tiles[map_coords.x + map_coords.y * g_map.w]))
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

b8 game_move_and_collide(vec2f* pos, vec2f* diff, f32 radius) {
  struct hit hit;
  f32 v_dist, h_dist;
  vec2f v_dir, h_dir;
  b8 collided = false;

  /* Check for collisions on the y-axis */
  v_dir.x = 0.0f;
  v_dir.y = signf(diff->y);
  v_dist = absf(diff->y);
  if (trace_ray(pos, &v_dir, COLL_DOF, &hit) &&
      hit.dist < v_dist + radius) {
    v_dist = hit.dist - radius;
    collided = true;
  }

  /* Check for collisions on the x-axis */
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

vec2f game_get_player_dir(void) {
  i32 long_dir, side_dir;
  vec2f dir = {
    .x = 0.0f, .y = 0.0f
  };

  long_dir = g_keys.forward - g_keys.backward;
  side_dir = g_keys.right - g_keys.left;

  /* Forward movement */
  if (long_dir) {
    dir.x += g_player.dir.x * long_dir;
    dir.y += g_player.dir.y * long_dir;
  }
  /* Sideways movement */
  if (side_dir) {
    dir.x += -g_player.dir.y * side_dir;
    dir.y += +g_player.dir.x * side_dir;

    /* If the user is trying to go both forwards and sideways,
     * we normalize the vector by dividing by sqrt(2).
     * We divide by sqrt(2) because both player.long_dir and player.side_dir
     * have a length of 1, therefore ||vec_long_dir + vec_side_dir|| = sqrt(2).
     */
    if (long_dir) {
      dir.x *= INV_SQRT2;
      dir.y *= INV_SQRT2;
    }
  }

  return dir;
}

static inline
void update_player_position(f32 delta) {
  vec2f dir;

  /* A dead man cannot move :^) */
  if (g_player.health <= 0)
    return;

  dir = game_get_player_dir();

  game_move_and_collide(&g_player.pos,
                        &VEC2SCALE(&dir, delta * PLAYER_RUN_SPEED),
                        g_sprite_radius[SPRITE_PLAYER]);
}

static inline
void update_sprites(f32 delta) {
  b8 collided;
  u32 i, j;
  f32 dist_from_other2, min_dist2;
  f32 s_radius;
  vec2f diff;
  struct sprite *s, *other;

  for (i = 0; i < g_sprites.n; ++i) {
    s = g_sprites.s + i;

    s_radius = g_sprite_radius[s->desc.type];

    collided = game_move_and_collide(&s->pos,
                                     &VEC2SCALE(&s->vel, delta),
                                     s_radius);
    /* Disable bullet sprites on collision with a wall */
    if (s->desc.type == SPRITE_BULLET)
      s->disabled = !s->disabled && collided;
    else /* s->desc.type == SPRITE_PLAYER */ {
      /* Do player sprite animation */
      if (s->anim_frame > 4.0f) {
        /* If anim_frame > 4, the firing frame is being shown */
        s->anim_frame -= delta * PLAYER_ANIM_FPS;
        if (s->anim_frame < 4.0f)
          s->anim_frame = 4.0f;
      }
      else if (VEC2LENGTH2(&s->vel) > 0.01f)
        /* Player is moving, animate */
        s->anim_frame = modf(s->anim_frame + delta * PLAYER_ANIM_FPS, 4.0f);
      else
        /* Player is standing, set the correct animation frame */
        s->anim_frame = 4.0f;
    }

    /* If the sprite is disabled or the sprite is not a player,
     * do not do collision checks with other sprites.
     */
    if (s->disabled || s->desc.type != SPRITE_PLAYER)
      continue;

    for (j = 0; j < g_sprites.n; ++j) {
      other = g_sprites.s + j;

      if (other->disabled || other == s)
        continue;

      /* Since we already need to compute dist_from_player2 we might as well
       * save the diff vector, because we'll also need it during rendering.
       */
      diff = VEC2SUB(&other->pos, &s->pos);

      /* Compute minimum distance between the two sprites */
      min_dist2 = s_radius + g_sprite_radius[other->desc.id];
      min_dist2 *= min_dist2;

      /* Compute the distance from the player and check if the
       * sprite collided.
       */
      dist_from_other2 = VEC2LENGTH2(&diff);
      if (dist_from_other2 < min_dist2) {
        if (other->desc.type == SPRITE_BULLET &&
            other->desc.owner != s->desc.id)
          /* Disable the bullet sprite if it collided with a player sprite */
          other->disabled = true;
      }
    }
  }

  UNUSED(delta);
}

void game_update(f32 delta) {
  update_player_position(delta);
  update_sprites(delta);
}

static
void draw_column(u8 cell_id, i32 x, const struct hit* hit) {
  i32 y, line_y, line_height;
  u32 line_color;

  /* Draw column */
  if (cell_id) {
    line_color = hit->vertical ? COLOR(WHITE) : COLOR(LIGHTGRAY);

    line_height = FB_HEIGHT / hit->dist;
    if ((u32)line_height > FB_HEIGHT)
      line_height = FB_HEIGHT;
    line_y = (FB_HEIGHT - line_height) >> 1;
  } else
    line_y = FB_HEIGHT >> 1;

  /* Fill column */
  y = 0;
  for (; y < line_y; ++y)
    fb_set_pixel(x, y, COLOR(BLUE));
  if (cell_id) {
    line_y += line_height;
    for (; y < line_y; ++y)
      fb_set_pixel(x, y, line_color);
  }
  for (; y < FB_HEIGHT; ++y)
    fb_set_pixel(x, y, COLOR(BLACK));
}

static inline
i32 get_y_end(struct sprite* s, u32 screen_h) {
  switch (s->desc.type) {
    case SPRITE_BULLET:
      /* We add a little offset to the bullet's vertical height so
       * that it doesn't come out of the player camera.
       */
      return (FB_HEIGHT + screen_h +
              (i32)((f32)BULLET_SCREEN_OFF * s->inv_depth)) >> 1;
    default:
      /* By default, place objects on the ground */
      return (f32)(FB_HEIGHT >> 1) * (1.0f + s->inv_depth);
  }
}

static inline
const u8* get_player_tile(u32 x, u32 y) {
  return &g_player_spritesheet[((y * PLAYER_NTILES_W) + x) *
                               (PLAYER_TILE_W * PLAYER_TILE_H)];
}

static
void get_texture_info(struct sprite* s, b8* invert_x, u32* tex_w, u32* tex_h,
                      const u8** tex, const u32** coltab) {
  i32 rot;

  *invert_x = false;

  /* Do sprite specific stuff */
  if (s->desc.type == SPRITE_PLAYER) {
    /* Set the player sprite data */
#define STEPS ((PLAYER_NTILES_H << 1) - 2)
#define SLICE (TWO_PI / STEPS)
    /* Determine the rotation of the sprite to use */
    rot = (s->rot + SLICE / 2.0f - s->rel_rot + PI) * STEPS / TWO_PI;
    rot &= 7;
    if (rot > 4) {
      rot = 8 - rot;
      *invert_x = true;
    }
    /* Get the pointer to the corresponding sprite texture */
    *tex = get_player_tile((u32)s->anim_frame, abs(rot));
    /* Set the color table pointer */
    *coltab = g_player_coltab;
    /* Set the texture width and height */
    *tex_w = PLAYER_TILE_W;
    *tex_h = PLAYER_TILE_H;
  } else /* s->desc.type == SPRITE_BULLET */ {
    /* Set the bullet sprite data */
    *tex = g_bullet_texture;
    *coltab = g_bullet_coltab;
    /* Set bullet texture dimensions */
    *tex_w = BULLET_TEXTURE_W;
    *tex_h = BULLET_TEXTURE_H;
  }
}

static
void draw_sprite(struct sprite* s) {
  u32 screen_h, color, a;
  u32 tex_w, tex_h;
  u32 uvw, uvh, uvx, uvy;
  i32 x_start, x_end, y_start, y_end;
  i32 x, y;
  const u8* tex;
  const u32* coltab;
  b8 invert_x = false;

  screen_h = (f32)g_sprite_dims[s->desc.type].y * s->inv_depth;

  /* Determine screen coordinates of the sprite */
  x_start = s->screen_x - s->screen_halfw;
  x_end = s->screen_x + s->screen_halfw;
  y_end = get_y_end(s, screen_h);
  y_start = y_end - screen_h;

  /* Compute the sprite width and height on the screen */
  uvw = MAX(x_end - x_start, 0);
  uvh = MAX(y_end - y_start, 0);

  get_texture_info(s, &invert_x, &tex_w, &tex_h, &tex, &coltab);

  a = color_get_alpha_mask();
  /* Draw the sprite */
  for (x = MAX(0, x_start); x < x_end && x < FB_WIDTH; x++) {
    /* Discard stripe if there's a wall closer to the camera */
    if (g_zbuf[x] < s->depth2)
      continue;
    /* Compute x texture coordinate */
    uvx = (f32)((x - x_start) * tex_w) / uvw;
    /* Invert on the x axis if needed */
    if (invert_x)
      uvx = tex_w - uvx;
    for (y = MAX(0, y_start); y < y_end && y < FB_HEIGHT; y++) {
      uvy = (f32)((y - y_start) * tex_h) / uvh;
      color = tex[uvx + uvy * tex_w];
      if (color)
        fb_set_pixel(x, y, coltab[color] | a);
    }
  }
}

static inline
void render_scene(void) {
  u8 cell_id;
  i32 x;
  f32 cam_x;
  vec2f ray_dir;
  struct hit hit;

  if (g_camera.smoothing) {
    /* Interpolate camera position with real position */
    g_camera.pos.x = lerp(CAMERA_POS_INTERP, g_camera.pos.x, g_player.pos.x);
    g_camera.pos.y = lerp(CAMERA_POS_INTERP, g_camera.pos.y, g_player.pos.y);
  } else
    g_camera.pos = g_player.pos;

  for (x = 0; x < FB_WIDTH; ++x) {
    /* Compute ray direction */
    cam_x = (2.0f * ((f32)x / FB_WIDTH)) - 1.0f;
    ray_dir.x = g_player.dir.x + g_camera.plane.x * cam_x;
    ray_dir.y = g_player.dir.y + g_camera.plane.y * cam_x;
    /* Trace ray with DDA */
    cell_id = trace_ray(&g_camera.pos, &ray_dir, g_camera.dof, &hit);
    /* Store distance (squared) in z-buffer */
    g_zbuf[x] = hit.dist * hit.dist;

    draw_column(cell_id, x, &hit);
  }
}

static inline
void render_sprites(void) {
  vec2f proj, diff, dir_to_s;
  u32 i, j, k, n;
  struct sprite *s, *on_screen_sprites[MAX_SPRITES];

  n = 0;
  for (i = 0; i < g_sprites.n; ++i) {
    s = g_sprites.s + i;

    /* Do not render disabled sprites */
    if (s->disabled)
      continue;

    /* Do not render the tracked sprite.
     * FIXME: Find a better way to do this maybe?
     */
    if (s == g_tracked_sprite)
      continue;

    diff = VEC2SUB(&s->pos, &g_camera.pos);

    /* Compute coordinates in camera space */
    proj.x = g_camera.inv_mat.m11 * diff.x + g_camera.inv_mat.m12 * diff.y;
    proj.y = g_camera.inv_mat.m21 * diff.x + g_camera.inv_mat.m22 * diff.y;

    /* Sprite is behind the camera, ignore it */
    if (proj.y < 0.0f)
      continue;

    /* Save camera depth */
    s->inv_depth = 1.0f / proj.y;

    /* Compute screen x */
    s->screen_x = (FB_WIDTH >> 1) * (1.0f + proj.x / proj.y);
    /* Compute screen width (we divide by two since
     * we always use the half screen width).
     */
    s->screen_halfw =
      (i32)((f32)g_sprite_dims[s->desc.type].x * s->inv_depth) >> 1;

    /* Sprite is not on screen, ignore it */
    if (s->screen_x + s->screen_halfw < 0 ||
        s->screen_x - s->screen_halfw >= FB_WIDTH)
      continue;

    s->depth2 = proj.y * proj.y;

    /* Compute angle relative to the player direction vector */
    if (s->desc.type == SPRITE_PLAYER) {
      /* Only do this for player sprites since they're the only sprites
       * that need it.
       */
      dir_to_s = vec2f_normalized(&diff);
      s->rel_rot =
        acos(dir_to_s.x * g_player.dir.x + dir_to_s.y * g_player.dir.y);
      s->rel_rot *= signf(proj.x);
      s->rel_rot += g_player.rot;
    }

    /* Look for index to insert the new sprite */
    for (j = 0; j < n; ++j) {
      if (on_screen_sprites[j]->depth2 < s->depth2)
        break;
    }
    /* Move elements after the entry to the next index */
    for (k = n; k > j; --k)
      on_screen_sprites[k] = on_screen_sprites[k-1];
    /* Store the new sprite to render */
    on_screen_sprites[j] = s;

    if (++n >= MAX_SPRITES)
      break;
  }

  for (i = 0; i < n; i++)
    draw_sprite(on_screen_sprites[i]);
}

static inline
u32 invert_color(u32 color) {
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

  return comps.u | color_get_alpha_mask();
}

static inline
void render_crosshair(void) {
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
        px = fb_get_pixel(x, y);
        px = invert_color(px);
        fb_set_pixel(x, y, px);
      }
    }
  }
}

static inline
void render_hud(void) {
  u32 health_bar_w, health_bar_c, x;
  b8 got_damage;
  const char health_lbl[] = "H";

  /* Check if the player received damage */
  got_damage = g_display_health != g_player.health;

  x = 8 + STRING_WIDTH_IMM(health_lbl) + 4;
  if (got_damage)
    /* If the player received damage, draw a rectangle around the health
     * bar to draw the attention of the player.
     */
    ui_draw_rect(4, 4, x + HEALTH_BAR_WIDTH, 8 + STRING_HEIGHT, COLOR(MAGENTA));

  health_bar_c = COLOR(RED);
  ui_draw_string_with_color(8, 8, health_lbl, health_bar_c);

  health_bar_w = (f32)(g_player.health * HEALTH_BAR_WIDTH) / PLAYER_MAX_HEALTH;
  ui_draw_rect(x, 8, health_bar_w, STRING_HEIGHT - 1, health_bar_c);

  if (got_damage) {
    /* If the player received damage, animate the health difference */
    x += health_bar_w;
    health_bar_w =
      (f32)((g_display_health - g_player.health) * HEALTH_BAR_WIDTH)
        / PLAYER_MAX_HEALTH;
    ui_draw_rect(x, 8, health_bar_w, STRING_HEIGHT - 1, COLOR(WHITE));

    g_display_health = lerp(HEALTH_BAR_LAG, g_player.health, g_display_health);
  }
}

static inline
void render_minimap(void) {
  u32 minimap_x, minimap_y;
  u32 i, j;
  u32 x, y;
  u8 cell_id;

  if (g_map.tiles == NULL)
    return;

  minimap_x = FB_WIDTH - (g_map.w * MINIMAP_TILE_W) - 4;
  minimap_y = 4;

  ui_draw_rect(minimap_x, minimap_y,
               g_map.w * MINIMAP_TILE_W, g_map.h * MINIMAP_TILE_H,
               COLOR(WHITE));

  for (i = 0; i < g_map.w; ++i) {
    for (j = 0; j < g_map.h; ++j) {
      cell_id = g_map.tiles[i + j * g_map.w];
      if (cell_id) {
        x = i * MINIMAP_TILE_W;
        y = j * MINIMAP_TILE_H;
        ui_draw_rect(minimap_x + x, minimap_y + y,
                     MINIMAP_TILE_W, MINIMAP_TILE_H,
                     COLOR(GRAY));
      }
    }
  }

  ui_draw_rect(
            minimap_x + g_player.pos.x * MINIMAP_TILE_W - (MINIMAP_TILE_W >> 2),
            minimap_y + g_player.pos.y * MINIMAP_TILE_H - (MINIMAP_TILE_H >> 2),
            MINIMAP_TILE_W >> 1, MINIMAP_TILE_H >> 1,
            COLOR(RED));
}

void game_render(void) {
  render_scene();
  render_sprites();
  render_crosshair();
  render_hud();
  render_minimap();
}

void game_init(void) {
  g_display_health = g_player.health = PLAYER_MAX_HEALTH;
  g_camera.pos = g_player.pos;
  /* If this seems counter intuitive, you should look into
   * the hello packet handler in multiplayer.c.
   */
  game_player_set_rot(g_player.rot);
}

void game_tick(f32 delta) {
  game_update(delta);
  game_render();
}
