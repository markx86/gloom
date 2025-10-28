#ifndef CLIENT_H_
#define CLIENT_H_

#include <gloom/types.h>

enum client_state {
  CLIENT_ERROR,
  CLIENT_LOADING,
  CLIENT_WAITING,
  CLIENT_GAME,
  CLIENT_PAUSE,
  CLIENT_OPTIONS,
  CLIENT_OVER,
  CLIENT_STATE_MAX,
};

extern b8 _g_pointer_locked;
extern enum client_state _g_client_state;

struct state_handlers {
  void (*on_tick)(f32);
  void (*on_enter)(void);
  void (*on_key)(u32, char, b8);
  void (*on_mouse_moved)(u32, u32, i32, i32);
  void (*on_mouse_down)(u32, u32, u32);
  void (*on_mouse_up)(u32, u32, u32);
  void (*on_pointer_lock_changed)(void);
};

static inline
b8 client_pointer_is_locked(void) {
  return _g_pointer_locked;
}

static inline
enum client_state client_get_state(void) {
  return _g_client_state;
}

void client_switch_state(enum client_state new_state);

#endif
