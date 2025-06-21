#ifndef __MULTIPLAYER_H__
#define __MULTIPLAYER_H__

#include <types.h>
#include <libc.h>

enum connection_state {
  CONN_DISCONNECTED,
  CONN_CONNECTED,
  CONN_JOINING,
  CONN_WAITING,
  CONN_UPDATING
};

extern enum connection_state __conn_state;

static inline b8 is_disconnected(void) {
  return __conn_state <= CONN_DISCONNECTED;
}

static inline b8 in_game(void) {
  return __conn_state >= CONN_WAITING;
}

static inline enum connection_state get_connection_state(void) {
  return __conn_state;
}

static inline void set_connection_state(enum connection_state state) {
  if (__conn_state != state) {
    printf("switching connection state from %d to %d\n", __conn_state, state);
    __conn_state = state;
  }
}

void display_game_id(void);
void queue_key_input(void);

void multiplayer_init(u32 gid, u32 token);

void signal_ready(b8 yes);
void leave_game(void);
void send_update(void);
void fire_bullet(void);

static inline void join_game(void) {
  signal_ready(false);
  set_connection_state(CONN_JOINING);
}

#endif
