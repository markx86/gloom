#include <types.h>
#include <libc.h>

enum connection_state {
  CONN_UNKNOWN,
  CONN_DISCONNECTED,
  CONN_CONNECTED,
  CONN_JOINING,
  CONN_UPDATING
};

void set_player_token(u32 token);
void set_online(b8 yes);

b8 is_disconnected(void);
b8 is_in_multiplayer_game(void);

enum connection_state get_connection_state(void);

void force_connection_state(enum connection_state state);

b8 join_game(u32 game_id);
b8 leave_game(void);

void send_update(void);
