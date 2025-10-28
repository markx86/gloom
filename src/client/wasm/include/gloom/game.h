#ifndef GAME_H_
#define GAME_H_

#include <gloom/math.h>

#define PLAYER_RUN_SPEED    3.5f
#define PLAYER_ROT_SPEED    0.01f

#define MAX_CAMERA_FOV 120.0f
#define MIN_CAMERA_FOV 30.0f

#define MAX_CAMERA_DOF 48
#define MIN_CAMERA_DOF 8

#define MAX_MOUSE_SENS 2.0f
#define MIN_MOUSE_SENS 0.1f

#define MAX_SPRITES   255

struct camera {
  u32 dof;
  f32 fov;
  f32 plane_halfw;
  vec2f pos;
  vec2f plane;
  struct {
    f32 m11, m12, m21, m22;
  } inv_mat;
  b8 smoothing;
};

struct player {
  f32 rot;
  vec2f pos;
  vec2f dir;
  i32 health;
};

enum sprite_type {
  SPRITE_PLAYER,
  SPRITE_BULLET,
  SPRITE_MAX
};

struct sprite_desc {
  u32 type  : 8; /* Sprite type */
  u32 id    : 8; /* Sprite ID */
  u32 owner : 8; /* Instantiator ID */
  u32 field : 8; /* Generic field used for extra data in requests */
};

struct sprite {
  struct sprite_desc desc;
  f32 rot;
  vec2f pos;
  vec2f vel;
  struct {
    b8 disabled;
    i32 screen_x;
    i32 screen_halfw;
    f32 inv_depth;
    f32 depth2;
    f32 anim_frame;
    f32 rel_rot;
  };
};

struct sprites {
  u32 n;
  struct sprite s[MAX_SPRITES];
};

struct map {
  u32 w, h;
  u8* tiles;
};

union keys {
  struct {
    b8 forward;
    b8 backward;
    b8 right;
    b8 left;
  };
  u32 all_keys;
};

struct hit {
  f32 dist;
  b8 vertical;
};

extern const f32 g_sprite_radius[SPRITE_MAX];
extern const vec2i g_sprite_dims[SPRITE_MAX];

#define PLAYER_MAX_HEALTH 100
#define BULLET_DAMAGE     25

void game_camera_set_fov(f32 new_fov);
void game_player_set_rot(f32 new_rot);
void game_player_add_rot(f32 delta);

vec2f game_get_player_dir(void);
b8    game_move_and_collide(vec2f* pos, vec2f* diff, f32 radius);

void game_init(void);
void game_tick(f32 delta);
void game_update(f32 delta);
void game_render(void);

#endif
