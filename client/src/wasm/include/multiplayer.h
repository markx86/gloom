#include <types.h>

enum connection_state {
  CONN_UNKNOWN,
  CONN_DISCONNECTED,
  CONN_CONNECTED,
  CONN_JOINING,
  CONN_UPDATING,
  CONN_LEAVING
};

extern enum connection_state __conn_state;

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

b8 join_game(u32 token, u32 game_id);
b8 leave_game(void);

void multiplayer_tick(void);
void send_update(void);
