#ifndef __GLOOM_H__
#define __GLOOM_H__

#include <types.h>
#include <math.h>

#define FB_WIDTH  640
#define FB_HEIGHT 480

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
  vec2f plane;
  struct {
    f32 m11, m12, m21, m22;
  } inv_mat;
};

struct player {
  f32 rot;
  vec2f pos;
  vec2f fake_pos;
  vec2f dir;
  i32 health;
};

enum sprite_type {
  SPRITE_PLAYER,
  SPRITE_BULLET,
  SPRITE_MAX
};

struct sprite_desc {
  u32 type  : 8; // sprite type
  u32 id    : 8; // sprite id
  u32 owner : 8; // instantiator id
  u32 field : 8; // generic field used for extra data in requests
};

struct sprite {
  struct sprite_desc desc;
  f32 rot;
  vec2f pos;
  vec2f vel;
  struct {
    i32 screen_x;
    i32 screen_halfw;
    f32 inv_depth;
    f32 depth2;
    b8 disabled;
    f32 anim_frame;
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

extern struct player player;
extern struct camera camera;
extern struct sprites sprites;
extern struct map map;
extern union keys keys;
extern const f32 sprite_radius[SPRITE_MAX];

#define PLAYER_MAX_HEALTH 100
#define BULLET_DAMAGE     25

void set_camera_fov(f32 new_fov);
void set_player_rot(f32 new_rot);

vec2f get_direction_from_keys(void);
b8 move_and_collide(vec2f* pos, vec2f* diff, f32 radius);

void gloom_init(void);
void gloom_tick(f32 delta);
void gloom_update(f32 delta);
void gloom_render(void);

static inline void off_player_rot(f32 delta) {
  f32 new_rot = player.rot + delta;
  if (new_rot >= TWO_PI)
    new_rot -= TWO_PI;
  else if (new_rot < 0.0f)
    new_rot += TWO_PI;
  set_player_rot(new_rot);
}

#endif
