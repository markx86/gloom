#ifndef MULTIPLAYER_H_
#define MULTIPLAYER_H_

#include <gloom/types.h>

enum multiplayer_state {
  MULTIPLAYER_DISCONNECTED,
  MULTIPLAYER_CONNECTED,
  MULTIPLAYER_JOINING,
  MULTIPLAYER_WAITING,
  MULTIPLAYER_UPDATING
};

extern enum multiplayer_state _g_multiplayer_state;

static inline
b8 multiplayer_is_disconnected(void) {
  return _g_multiplayer_state == MULTIPLAYER_DISCONNECTED;
}

static inline
b8 multiplayer_is_in_game(void) {
  return _g_multiplayer_state == MULTIPLAYER_UPDATING;
}

static inline
enum multiplayer_state multiplayer_get_state(void) {
  return _g_multiplayer_state;
}

void multiplayer_set_state(enum multiplayer_state state);

void multiplayer_draw_game_id(void);
void multiplayer_queue_key_input(void);

void multiplayer_init(u32 gid, u32 token);

void multiplayer_signal_ready(b8 yes);
void multiplayer_leave(void);
void multiplayer_send_update(void);
void multiplayer_fire_bullet(void);

static inline
void multiplayer_join_game(void) {
  multiplayer_signal_ready(false);
  multiplayer_set_state(MULTIPLAYER_JOINING);
}

#endif
