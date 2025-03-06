#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <gloom.h>
#include <multiplayer.h>
#include <libc.h>

extern u32 fb[FB_WIDTH * FB_HEIGHT];
extern b8 pointer_locked;

#define FB_SIZE   sizeof(fb)
#define FB_LEN    ARRLEN(fb)

enum client_state {
  STATE_MENU,
  STATE_LOADING,
  STATE_GAME,
  STATE_PAUSE,
  STATE_OPTIONS,
  STATE_ABOUT,
  STATE_MAX,
};

#define KEY_A 65
#define KEY_D 68
#define KEY_S 83
#define KEY_W 87
#define KEY_P 80

struct state_handlers {
  void (*on_tick)(f32);
  void (*on_enter)(enum client_state);
  void (*on_key)(u32, char, b8);
  void (*on_mouse_moved)(u32, u32, i32, i32);
  void (*on_mouse_down)(u32, u32, u32);
  void (*on_mouse_up)(u32, u32, u32);
  void (*on_pointer_lock_changed)(void);
};

void switch_to_state(enum client_state state);

#endif
