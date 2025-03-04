#include <types.h>
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

#define PLAYER_RUN_SPEED    3.5f
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

struct player {
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
};

extern struct player player;
extern struct camera camera;
extern struct map map;
extern struct sprite sprites[2];
extern struct keys keys;
extern f32 z_buf[FB_WIDTH];

u8 trace_ray(const vec2f* ray_dir, struct hit* hit);

void update(f32 delta);

void set_camera_fov(f32 new_fov);
void set_player_rot(f32 new_rot);

static inline void off_player_rot(f32 delta) {
  f32 new_rot = player.rot + delta;
  if (new_rot >= TWO_PI)
    new_rot -= TWO_PI;
  else if (new_rot < 0.0f)
    new_rot += TWO_PI;
  set_player_rot(new_rot);
}
