#include <types.h>
#include <math.h>

#define FB_WIDTH  640
#define FB_HEIGHT 480

#define PLAYER_RUN_SPEED    3.5f
#define PLAYER_ROT_SPEED    0.01f

#define CAMERA_FOV 75.0f
#define CAMERA_DOF 32

#define MAX_SPRITES   256
#define SPRITE_RADIUS 0.15f

enum color {
  COLOR_BLACK,
  COLOR_GRAY,
  COLOR_LIGHTGRAY,
  COLOR_WHITE,
  COLOR_DARKRED,
  COLOR_RED,
  COLOR_DARKGREEN,
  COLOR_GREEN,
  COLOR_DARKYELLOW,
  COLOR_YELLOW,
  COLOR_DARKBLUE,
  COLOR_BLUE,
  COLOR_DARKMAGENTA,
  COLOR_MAGENTA,
  COLOR_DARKCYAN,
  COLOR_CYAN,
  COLOR_MAX
};

#define color(x)       get_color(COLOR_##x)
#define solid_color(x) get_solid_color(COLOR_##x)

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
  vec2f dir;
};

enum sprite_type {
  SPRITE_PLAYER,
  SPRITE_BULLET,
  SPRITE_MAX
};

struct sprite_it {
  u16 type : 8;
  u16 id   : 8;
};

struct sprite {
  struct sprite_it it;
  f32 rot;
  f32 vel;
  vec2f pos;
  vec2f dir;
  struct {
    i32 screen_x;
    i32 screen_halfw;
    f32 inv_depth;
    f32 depth2;
    b8 disabled;
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

extern u32 __alpha_mask;
extern const u32 __palette[COLOR_MAX];

extern struct player player;
extern struct camera camera;
extern struct sprites sprites;
extern struct map map;
extern union keys keys;
extern f32 z_buf[FB_WIDTH];
extern u32 fb[FB_WIDTH * FB_HEIGHT];

#define PLAYER_SPRITE_W 128
#define PLAYER_SPRITE_H 400

#define BULLET_SPRITE_W 32
#define BULLET_SPRITE_H 32

#define FB_SIZE   sizeof(fb)
#define FB_LEN    ARRLEN(fb)

static inline void set_alpha(u8 a) {
  __alpha_mask = ((u32)a) << 24;
}

static inline u32 get_color(u8 index) {
  return __alpha_mask | __palette[index];
}

static inline u32 get_solid_color(u8 index) {
  return 0xFF000000 | __palette[index];
}

static inline struct sprite* alloc_sprite(void) {
  return (sprites.n < MAX_SPRITES) ? &sprites.s[sprites.n++] : NULL;
}

void set_camera_fov(f32 new_fov);
void set_player_rot(f32 new_rot);

void gloom_tick(f32 delta);

static inline void off_player_rot(f32 delta) {
  f32 new_rot = player.rot + delta;
  if (new_rot >= TWO_PI)
    new_rot -= TWO_PI;
  else if (new_rot < 0.0f)
    new_rot += TWO_PI;
  set_player_rot(new_rot);
}
