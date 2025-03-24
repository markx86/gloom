#include <types.h>
#include <libc.h>

enum connection_state {
  CONN_UNKNOWN,
  CONN_DISCONNECTED,
  CONN_CONNECTED,
  CONN_JOINING,
  CONN_UPDATING
};

extern enum connection_state __conn_state;

void set_player_token(u32 token);

static inline void set_online(b8 yes) {
  if (__conn_state == CONN_UNKNOWN)
    __conn_state = yes ? CONN_CONNECTED : CONN_DISCONNECTED;
}

static inline b8 is_disconnected(void) {
  return __conn_state <= CONN_DISCONNECTED;
}

static inline b8 is_in_multiplayer_game(void) {
  return __conn_state == CONN_UPDATING;
}

static inline enum connection_state get_connection_state(void) {
  return __conn_state;
}

static inline void force_connection_state(enum connection_state state) {
  if (__conn_state != state) {
    printf("forcing connection state from %d to %d\n", __conn_state, state);
    __conn_state = state;
  }
}

b8 join_game(u32 game_id);
b8 leave_game(void);

void send_update(void);
