#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <gloom.h>
#include <multiplayer.h>
#include <libc.h>

enum client_state {
  STATE_ERROR,
  STATE_MENU,
  STATE_LOADING,
  STATE_WAITING,
  STATE_GAME,
  STATE_PAUSE,
  STATE_OPTIONS,
  STATE_ABOUT,
  STATE_OVER,
  STATE_MAX,
};

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

extern const u32 __palette[COLOR_MAX];
extern u32 __alpha_mask;
extern u32 __fb[FB_WIDTH * FB_HEIGHT];
extern b8 __pointer_locked;

#define FB_SIZE   sizeof(__fb)
#define FB_LEN    ARRLEN(__fb)

static inline void set_alpha(u8 a) {
  __alpha_mask = ((u32)a) << 24;
}

static inline u32 get_alpha_mask(void) {
  return __alpha_mask;
}

static inline u32 get_color(u8 index) {
  return __alpha_mask | __palette[index];
}

static inline u32 get_solid_color(u8 index) {
  return 0xFF000000 | __palette[index];
}

#define COLOR(x)       get_color(COLOR_##x)
#define SOLIDCOLOR(x)  get_solid_color(COLOR_##x)

extern enum client_state __client_state;

struct state_handlers {
  void (*on_tick)(f32);
  void (*on_enter)(void);
  void (*on_key)(u32, char, b8);
  void (*on_mouse_moved)(u32, u32, i32, i32);
  void (*on_mouse_down)(u32, u32, u32);
  void (*on_mouse_up)(u32, u32, u32);
  void (*on_pointer_lock_changed)(void);
};

static inline void set_pixel(u32 x, u32 y, u32 c) {
  __fb[x + y * FB_WIDTH] = c;
}

static inline void set_pixel_index(u32 i, u32 c) {
  __fb[i] = c;
}

static inline u32 get_pixel(u32 x, u32 y) {
  return __fb[x + y * FB_WIDTH];
}

static inline u32 get_pixel_index(u32 i) {
  return __fb[i];
}

static inline b8 pointer_is_locked(void) {
  return __pointer_locked;
}

static inline enum client_state get_client_state(void) {
  return __client_state;
}

void switch_to_state(enum client_state new_state);
void exit(void);

#endif
