#include <gloom/multiplayer.h>
#include <gloom/game.h>
#include <gloom/client.h>
#include <gloom/globals.h>
#include <gloom/ui.h>

#define MAX_PACKET_DROP 10U

enum game_pkt_type {
  GPKT_READY,
  GPKT_LEAVE,
  GPKT_UPDATE,
  GPKT_FIRE,
  GPKT_MAX
};

struct game_pkt_hdr {
  u32 seq  : 30;
  u32 type : 2;
  u32 player_token;
} PACKED;

#define DEFINE_GPKT(name, body) \
  struct game_pkt_##name {      \
    struct game_pkt_hdr hdr;    \
    struct body PACKED;         \
  } PACKED

DEFINE_GPKT(ready, {
  b8 yes;
});

DEFINE_GPKT(leave, {});

DEFINE_GPKT(update, {
  u32 keys;
  f32 rot;
  f32 ts;
});

DEFINE_GPKT(fire, {});

enum serv_pkt_type {
  SPKT_HELLO,
  SPKT_UPDATE,
  SPKT_CREATE,
  SPKT_DESTROY,
  SPKT_WAIT,
  SPKT_TERMINATE,
  SPKT_MAX
};

struct serv_pkt_hdr {
  u32 seq  : 29;
  u32 type : 3;
} PACKED;

struct sprite_transform {
  f32 rot;
  vec2f pos;
  vec2f vel;
} PACKED;

struct sprite_init {
  struct sprite_desc desc;
  struct sprite_transform transform;
} PACKED;

#define DEFINE_SPKT(name, body) \
  struct serv_pkt_##name {      \
    struct serv_pkt_hdr hdr;    \
    struct body PACKED;         \
  } PACKED

DEFINE_SPKT(hello, {
  u8 n_sprites;
  u8 player_id;
  u32 map_w;
  u32 map_h;
  u8 data[0];
});

DEFINE_SPKT(update, {
  f32 ts;
  u8 id;
  struct sprite_transform transform;
});

DEFINE_SPKT(create, {
  struct sprite_init sprite;
});

DEFINE_SPKT(destroy, {
  struct sprite_desc desc;
});

DEFINE_SPKT(wait, {
  u32 seconds : 31;
  u32 wait    : 1;
});

DEFINE_SPKT(death, {});

DEFINE_SPKT(terminate, {});

struct input_log {
  f32 ts;
  vec2f vel;
};

#define RING_SIZE 128

struct {
  struct input_log *_tail, *_head;
  struct input_log _buffer[RING_SIZE];
} iring;

static
void iring_init(void) {
  iring._tail = iring._buffer;
  iring._head = iring._buffer;
}

static
void iring_push_elem(f32 ts, vec2f* vel) {
  iring._head->ts = ts;
  iring._head->vel = *vel;
  if (++iring._head >= iring._buffer + RING_SIZE)
    iring._head = iring._buffer;
  if (iring._head == iring._tail)
    ++iring._tail;
}

static
void iring_set_tail(struct input_log* ilog) {
  if ((u32)(ilog - iring._buffer) < RING_SIZE)
    iring._tail = ilog;
}

static
struct input_log* iring_get_next(void) {
  struct input_log* ilog;
  if (iring._tail == iring._head)
    return NULL;
  ilog = iring._tail;
  if (++iring._tail >= iring._buffer + RING_SIZE)
    iring._tail = iring._buffer;
  return ilog;
}

static
struct input_log* iring_get_after(struct input_log* ilog) {
  if (++ilog >= iring._buffer + RING_SIZE)
    ilog = iring._buffer;
  return ilog == iring._head ? NULL : ilog;
}

static u8 player_id;
static u32 game_id, player_token;
static u32 client_seq, server_seq;
static f32 game_start;
static char pkt_buf[0x1000];

void* gloom_packet_buffer(void) {
  return pkt_buf;
}

u32 gloom_packet_buffer_size(void) {
  return sizeof(pkt_buf);
}

// draw game id in the bottom-right corner
void display_game_id(void) {
  char gids[32];
  u32 x, w;
  const u32 y = FB_HEIGHT - STRING_HEIGHT - 32;
  snprintf(gids, sizeof(gids), "GAME ID: %x", game_id);
  w = STRING_WIDTH(gids);
  x = FB_WIDTH - 32 - w;
  ui_draw_rect(x - 2, y - 2, w + 4, STRING_HEIGHT + 2, SOLID_COLOR(DARKRED));
  ui_draw_string_with_color(x, y, gids, SOLID_COLOR(LIGHTGRAY));
}

enum multiplayer_state _multiplayer_state;

typedef void (*serv_pkt_handler_t)(void*, u32);

static inline
f32 get_ts(void) {
  return platform_get_time() - game_start;
}

static
void init_game_pkt(void* hdrp, enum game_pkt_type type) {
  struct game_pkt_hdr* hdr = hdrp;
  hdr->type = type;
  hdr->seq = client_seq++;
  hdr->player_token = player_token;
}

static
void send_packet_checked(void* pkt, u32 size) {
  if (platform_send_packet(pkt, size) != (i32)size)
    multiplayer_set_state(MULTIPLAYER_DISCONNECTED);
}

void multiplayer_init(u32 gid, u32 token) {
  game_id = gid;
  player_token = token;
  // reset game packet sequence
  client_seq = server_seq = 0;
  iring_init();
  multiplayer_set_state(MULTIPLAYER_CONNECTED);
}

void queue_key_input(void) {
  vec2f vel = game_get_player_dir();
  iring_push_elem(get_ts(), &VEC2SCALE(&vel, PLAYER_RUN_SPEED));
}

void multiplayer_signal_ready(b8 yes) {
  struct game_pkt_ready pkt;
  init_game_pkt(&pkt, GPKT_READY);
  pkt.yes = yes;
  send_packet_checked(&pkt, sizeof(pkt));
}

void multiplayer_leave(void) {
  struct game_pkt_leave pkt;
  free_all(); // free map data
  init_game_pkt(&pkt, GPKT_LEAVE);
  multiplayer_set_state(MULTIPLAYER_CONNECTED);
  send_packet_checked(&pkt, sizeof(pkt));
}

void multiplayer_send_update(void) {
  struct game_pkt_update pkt;
  init_game_pkt(&pkt, GPKT_UPDATE);
  pkt.keys = g_keys.all_keys;
  pkt.rot = g_player.rot;
  pkt.ts = get_ts();
  send_packet_checked(&pkt, sizeof(pkt));
}

void multiplayer_fire_bullet(void) {
  struct game_pkt_fire pkt;
  init_game_pkt(&pkt, GPKT_FIRE);
  platform_send_packet(&pkt, sizeof(pkt));
}

static
void pkt_size_error(const char* pkt_type, u32 got, u32 expected) {
  eprintf("%s packet size is not what was expected (should be %u, got %u)\n",
          pkt_type, expected, got);
}

static
void pkt_type_error(const char* pkt_type) {
  eprintf("received %s packet, but the connection state is wrong (now in %d)\n",
          pkt_type, multiplayer_get_state());
}

static
void apply_sprite_transform(struct sprite* s, struct sprite_transform* t) {
  s->rot = t->rot;
  s->pos = t->pos;
  s->vel = t->vel;
  // since we have computed the inverse of the velocity,
  // we have to do this inversion to obtain the normat velocity
  s->disabled = false; // reset disabled flag
}

static inline
struct sprite* alloc_sprite(void) {
  return (g_sprites.n < MAX_SPRITES) ? &g_sprites.s[g_sprites.n++] : NULL;
}

static
u32 count_player_sprites(void) {
  u32 i, n = 0;
  for (i = 0; i < g_sprites.n; ++i)
    n += (g_sprites.s[i].desc.type == SPRITE_PLAYER);
  return n;
}

// get a pointer to the sprite with the requested @id
// if no sprite with that id exists and @can_alloc is true,
// a new sprite with that id will be allocated
static
struct sprite* get_sprite(u8 id, b8 can_alloc) {
  struct sprite* s;
  u32 i = 0;
  for (i = 0; i < g_sprites.n; ++i) {
    s = &g_sprites.s[i];
    if (s->desc.id == id)
      return s;
  }
  if (can_alloc && (s = alloc_sprite()))
    return s;
  return NULL;
}

static inline
void track_sprite(u8 id) {
  g_tracked_sprite_set(get_sprite(id, false));
}

// remove sprite with the requested @id
static
void destroy_sprite(u8 id) {
  u32 i, tid;
  struct sprite *tracked_sprite, *s = get_sprite(id, false);
  if (s == NULL)
    return;

  printf("destroying sprite %u (type %u)\n", s->desc.id, s->desc.type);
  tracked_sprite = g_tracked_sprite_get();

  // save the id of the tracked if we're going to shift it
  tid = tracked_sprite != NULL && tracked_sprite->desc.id > s->desc.id ?
        tracked_sprite->desc.id : 0;
  for (i = (u32)(s - g_sprites.s) + 1; i < g_sprites.n; ++i)
    g_sprites.s[i - 1] = g_sprites.s[i];
  --g_sprites.n;
  // update sprite tracker pointer if the position in the array changed
  if (tid > 0)
    track_sprite(tid);
}

// initialize sprite from packet data
static
void init_sprite(struct sprite_init* init) {
  struct sprite* s;
  // check if the sprite type is valid
  if (init->desc.type >= SPRITE_MAX)
    return;
  // get or allocate the requested sprite
  if ((s = get_sprite(init->desc.id, true))) {
    printf("creating sprite with id %u (type %u)\n",
           init->desc.id, init->desc.type);
    // initialize the sprite struct with the provided data
    memset(s, 0, sizeof(*s));
    s->desc = init->desc;
    apply_sprite_transform(s, &init->transform);
    // if a bullet was fired, play the correct animation for that sprite
    // FIXME: maybe this shouldn't be done in this function?
    if (s->desc.type == SPRITE_BULLET &&
        (s = get_sprite(init->desc.owner, false)))
      s->anim_frame = 6.0f;
  }
}

static
void serv_hello_handler(void* buf, u32 len) {
  u32 expected_pkt_len, sprites_size, map_size;
  u32 n_sprites, x, y, i, j;
  u8 *m, b;
  struct sprite_init* s;
  struct serv_pkt_hello* pkt = buf;

  // check the connection state
  if (multiplayer_get_state() != MULTIPLAYER_JOINING) {
    pkt_type_error("hello");
    return;
  }

  n_sprites = pkt->n_sprites;

  map_size = (pkt->map_h * pkt->map_w + 7) >> 3; // compute the size of the map data
  sprites_size = n_sprites * sizeof(*s); // compute the size of the sprite data

  // check the packet size is correct
  expected_pkt_len = sizeof(*pkt) + map_size + sprites_size;
  if (expected_pkt_len != len) {
    pkt_size_error("hello", len, expected_pkt_len);
    return; // malformed packet, drop it
  }

  // get the size of variable data (map data + sprite data),
  // remove the size of the packet header from the packet length
  len -= sizeof(*pkt);

  // initialize sprites array and map struct
  g_sprites.n = 0;
  g_map.w = pkt->map_w;
  g_map.h = pkt->map_h;
  g_map.tiles = malloc(map_size);

  // set player id
  player_id = pkt->player_id;

  s = (struct sprite_init*)pkt->data;
  m = pkt->data + sizeof(*s) * n_sprites;

  // process sprite data
  if (n_sprites > 0) {
    for (; n_sprites > 0; --n_sprites) {
      if (len < sizeof(*s))
        break;
      if (s->desc.id != player_id)
        init_sprite(s);
      else {
        // init data refers to the player
        g_player.pos = s->transform.pos;
        g_player.rot = s->transform.rot;
      }
      len -= sizeof(*s);
      ++s;
    }
  }
  // process map data
  if (n_sprites == 0) {
    x = y = 0;
    for (i = 0; i < len; ++i) {
      b = m[i];
      for (j = 0; j < 8; ++j) {
        g_map.tiles[x + y * g_map.w] = b & 1;
        b >>= 1;
        if (++x == g_map.w) {
          x = 0;
          ++y;
        }
      }
    }
  }

  multiplayer_set_state(MULTIPLAYER_WAITING);
}

static inline
void reconcile(f32 ts, vec2f* pos, vec2f* vel) {
  f32 delta;
  struct input_log* ilog;
  vec2f diff;
  const f32 radius = g_sprite_radius[SPRITE_PLAYER];

  // discard all old logs
  for (ilog = iring_get_next(); ilog != NULL; ilog = iring_get_next()) {
    if (ts < ilog->ts)
      break;
  }

  // step through all past events and recompute the current player position
  if (ilog != NULL) {
    iring_set_tail(ilog);

    for (; ilog != NULL; ilog = iring_get_after(ilog)) {
      delta = ilog->ts - ts;
      diff = VEC2SCALE(vel, delta);
      game_move_and_collide(pos, &diff, radius);
      vel = &ilog->vel;
      ts = ilog->ts;
    }
  }

  delta = get_ts() - ts;
  diff = VEC2SCALE(vel, delta);
  game_move_and_collide(pos, &diff, radius);

  // set recomputed player position
  g_player.pos = *pos;
}

static
void serv_update_handler(void* buf, u32 len) {
  struct sprite* s;
  struct sprite_transform* t;
  struct serv_pkt_update* pkt = buf;

  // check connection state
  if (multiplayer_get_state() != MULTIPLAYER_UPDATING) {
    pkt_type_error("update");
    return;
  }

  // check packet size
  if (sizeof(*pkt) != len) {
    pkt_size_error("update", len, sizeof(*pkt));
    return;
  }

  // process update data
  t = &pkt->transform;
  if (pkt->id == player_id)
    // update data refers to the player
    reconcile(pkt->ts, &t->pos, &t->vel);
  else if ((s = get_sprite(pkt->id, false)))
    apply_sprite_transform(s, t);
}

static
void serv_create_handler(void* buf, u32 len) {
  struct serv_pkt_create* pkt = buf;

  // check connection state (can be CONN_UPDATING or CONN_WAITING)
  if (multiplayer_get_state() != MULTIPLAYER_UPDATING &&
      multiplayer_get_state() != MULTIPLAYER_WAITING) {
    pkt_type_error("create");
    return;
  }

  // check packet size
  if (sizeof(*pkt) != len) {
    pkt_size_error("create", len, sizeof(*pkt));
    return;
  }

  // initialize sprite
  init_sprite(&pkt->sprite);
}

static
void serv_destroy_handler(void* buf, u32 len) {
  struct sprite* tracked_sprite;
  struct serv_pkt_destroy* pkt = buf;

  // check connection state (can be CONN_UPDATING or CONN_WAITING)
  if (multiplayer_get_state() != MULTIPLAYER_UPDATING &&
      multiplayer_get_state() != MULTIPLAYER_WAITING) {
    pkt_type_error("destroy");
    return;
  }

  // check packet size
  if (sizeof(*pkt) != len) {
    pkt_size_error("destroy", len, sizeof(*pkt));
    return;
  }

  if (pkt->desc.id == player_id) {
    // destroy packet refers to the player
    client_switch_state(CLIENT_OVER);
    // make sprite tracker follow the player sprite that "killed" the player
    track_sprite(pkt->desc.field);
    // clear the player sprite id, since once the player is dead the server will
    // reuse they're sprite id for other sprites
    player_id = 0;
    return;
  } else if ((tracked_sprite = g_tracked_sprite_get()) != NULL &&
             pkt->desc.id == tracked_sprite->desc.id)
    // if the tracked sprite is "killed", follow the "killer"
    track_sprite(pkt->desc.field);

  destroy_sprite(pkt->desc.id);

  // handle bullet points and damage
  if (pkt->desc.type == SPRITE_BULLET && pkt->desc.field != 0) {
    if (pkt->desc.owner == player_id)
      ; // TODO: add player reward
    else if (pkt->desc.field == player_id)
      g_player.health -= BULLET_DAMAGE;
  }

  // if the number of players left is 0 and we are in game,
  // switch to the game over screen
  if (pkt->desc.type == SPRITE_PLAYER &&
      client_get_state() != CLIENT_WAITING &&
      count_player_sprites() == 0) {
    client_switch_state(CLIENT_OVER);
  }
}

static
void serv_wait_handler(void* buf, u32 len) {
  struct serv_pkt_wait* pkt = buf;

  // check connection state
  if (multiplayer_get_state() != MULTIPLAYER_WAITING) {
    pkt_type_error("wait");
    return;
  }

  // check packet size
  if (sizeof(*pkt) != len) {
    pkt_size_error("wait", len, sizeof(*pkt));
    return;
  }

  // NOTE: pkt->wait is a boolean that indicates whether the game has
  // reached the minimum amount of players necessary to start (false)
  // or not (true)

  if (!pkt->wait && pkt->seconds == 0) {
    // if the wait flag is false (the game has the minimum amount of players)
    // and the seconds left to wait are 0, switch to game state
    multiplayer_set_state(MULTIPLAYER_UPDATING);
    game_start = platform_get_time(); // set the game start time
  }
  else
    // if the wait flag is true (the game has not reached the minimum amount of
    // players, yet) wait an infinite time (-1.0f means infinite wait),
    // otherwise initialize the wait timer to the number of seconds requested
    // by the server
    g_wait_time_set(pkt->wait ? -1.0f : (f32)pkt->seconds);
}

static
void serv_terminate_handler(void* buf, u32 len) {
  struct serv_pkt_terminate* pkt = buf;

  // NOTE: this packet can be sent by the server in ANY connection state

  // check packet length
  if (sizeof(*pkt) != len) {
    pkt_size_error("terminate", len, sizeof(*pkt));
    return;
  }

  // switch to disconnected state
  multiplayer_set_state(MULTIPLAYER_DISCONNECTED);
  // if the client is not in the game over state, that means an error has
  // occurred on the server side, so we should display the error screen
  if (client_get_state() != CLIENT_OVER)
    client_switch_state(CLIENT_ERROR);
}

static const serv_pkt_handler_t serv_pkt_handlers[SPKT_MAX] = {
  [SPKT_HELLO]   = serv_hello_handler,
  [SPKT_UPDATE]  = serv_update_handler,
  [SPKT_CREATE]  = serv_create_handler,
  [SPKT_DESTROY] = serv_destroy_handler,
  [SPKT_WAIT]    = serv_wait_handler,
  [SPKT_TERMINATE] = serv_terminate_handler
};

void gloom_on_recv_packet(u32 len) {
  struct serv_pkt_hdr* hdr;

  // check that the packet fits inside the packet buffer
  if (len > sizeof(pkt_buf)) {
    eprintf("packet too big! (max. packet size is %u bytes, but got %u)\n",
            sizeof(pkt_buf), len);
    return;
  }

  hdr = (struct serv_pkt_hdr*)pkt_buf;
  if (!hdr)
    return; // no message received or recv error
  if (len < sizeof(*hdr)) {
    pkt_size_error("server", len, sizeof(*hdr));
    return; // no data?
  }

  // ensure sequence number is within margin of error
  if (abs(hdr->seq - server_seq) < MAX_PACKET_DROP)
    server_seq = hdr->seq + 1;
  else {
    eprintf("invalid sequence number (expected %u got %u)\n",
            server_seq, hdr->seq);
    return;
  }

  // ensure the packet type is valid
  if (hdr->type >= SPKT_MAX) {
    eprintf("unknown packet type (got %u)\n", hdr->type);
    return; // unknown packet type, drop it
  }

  // call the appropriate packet handler
  serv_pkt_handlers[hdr->type](hdr, len);
}
