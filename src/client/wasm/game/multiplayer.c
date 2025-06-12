#include <multiplayer.h>
#include <gloom.h>
#include <client.h>
#include <globals.h>
#include <ui.h>

#define PACKED       __attribute__((packed))
#define WS_PORT      8492

#define MAX_PACKET_DROP 10

enum game_pkt_type {
  GPKT_JOIN,
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

DEFINE_GPKT(join, {
  u32 game_id;
});

DEFINE_GPKT(leave, {});

DEFINE_GPKT(update, {
  f32 ts;
  vec2f pos;
  f32 rot;
  u32 keys;
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

struct sprite_update {
  u8 id;
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
  struct sprite_update update;
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

static u8 player_id;
static u32 game_id, player_token;
static u32 client_seq, server_seq;
static f32 game_start;

void display_game_id(void) {
  char gids[32];
  u32 x, w;
  const u32 y = FB_HEIGHT - STRING_HEIGHT - 32;
  snprintf(gids, sizeof(gids), "GAME ID: %x", game_id);
  w = STRING_WIDTH(gids);
  x = FB_WIDTH - 32 - w;
  draw_rect(x - 2, y - 2, w + 4, STRING_HEIGHT + 2, SOLIDCOLOR(DARKRED));
  draw_string_with_color(x, y, gids, SOLIDCOLOR(LIGHTGRAY));
}

// NOTE: pkt_buf is accessed from JS, when changing the size remember to also
//       change it in app.js
char __pkt_buf[0x1000];
enum connection_state __conn_state;

typedef void (*serv_pkt_handler_t)(void*, u32);

static void init_game_pkt(void* hdrp, enum game_pkt_type type) {
  struct game_pkt_hdr* hdr = hdrp;
  hdr->type = type;
  hdr->seq = client_seq++;
  hdr->player_token = player_token;
}

static void send_packet_checked(void* pkt, u32 size) {
  if (send_packet(pkt, size) != (i32)size)
    set_connection_state(CONN_DISCONNECTED);
}

void multiplayer_init(u32 gid, u32 token) {
  game_id = gid;
  player_token = token;
  // reset game packet sequence
  client_seq = server_seq = 0;
  set_connection_state(CONN_CONNECTED);
}

void join_game(void) {
  struct game_pkt_join pkt;
  init_game_pkt(&pkt, GPKT_JOIN);
  pkt.game_id = game_id;
  set_connection_state(CONN_JOINING);
  send_packet_checked(&pkt, sizeof(pkt));
}

void leave_game(void) {
  struct game_pkt_leave pkt;
  init_game_pkt(&pkt, GPKT_LEAVE);
  set_connection_state(CONN_CONNECTED);
  send_packet_checked(&pkt, sizeof(pkt));
  free_all(); // free map data
}

void send_update(void) {
  struct game_pkt_update pkt;
  init_game_pkt(&pkt, GPKT_UPDATE);
  pkt.ts = time() - game_start;
  pkt.pos = player.pos;
  pkt.rot = player.rot;
  pkt.keys = keys.all_keys;
  send_packet_checked(&pkt, sizeof(pkt));
}

void fire_bullet(void) {
  struct game_pkt_fire pkt;
  init_game_pkt(&pkt, GPKT_FIRE);
  send_packet(&pkt, sizeof(pkt));
}

static void pkt_size_error(const char* pkt_type, u32 got, u32 expected) {
  eprintf("%s packet size is not what was expected (should be %u, got %u)\n",
          pkt_type, expected, got);
}

static void pkt_type_error(const char* pkt_type) {
  eprintf("received %s packet, but the connection state is wrong (now in %d)\n",
          pkt_type, get_connection_state());
}

static void apply_sprite_transform(struct sprite* s,
                                   struct sprite_transform* t) {
  f32 inv_vel = inv_sqrt(t->vel.x * t->vel.x + t->vel.y * t->vel.y);
  s->rot = t->rot;
  s->pos = t->pos;
  s->dir.x = t->vel.x * inv_vel;
  s->dir.y = t->vel.y * inv_vel;
  s->vel = 1.0f / inv_vel;
  s->disabled = false; // reset disabled flag
}

static inline struct sprite* alloc_sprite(void) {
  return (sprites.n < MAX_SPRITES) ? &sprites.s[sprites.n++] : NULL;
}

static struct sprite* get_sprite(u8 id, b8 can_alloc) {
  struct sprite* s;
  u32 i = 0;
  for (i = 0; i < sprites.n; ++i) {
    s = &sprites.s[i];
    if (s->desc.id == id)
      return s;
  }
  if (can_alloc && (s = alloc_sprite()))
    return s;
  return NULL;
}

static void destroy_sprite(u8 id) {
  u32 i;
  struct sprite* s = get_sprite(id, false);
  if (!s)
    return;
  for (i = (u32)(s - sprites.s) + 1; i < sprites.n; ++i)
    sprites.s[i - 1] = sprites.s[i];
  --sprites.n;
}

static void init_sprite(struct sprite_init* init) {
  struct sprite* s;
  printf("initializing sprite: %u\n", init->desc.id);
  if (init->desc.type >= SPRITE_MAX)
    return;
  if ((s = get_sprite(init->desc.id, true))) {
    memset(s, 0, sizeof(*s));
    s->desc = init->desc;
    apply_sprite_transform(s, &init->transform);
    // if a bullet was fired, play the correct animation for that sprite
    if (s->desc.type == SPRITE_BULLET &&
        (s = get_sprite(init->desc.owner, false)))
      s->anim_frame = 6.0f;
  }
}

static void serv_hello_handler(void* buf, u32 len) {
  u32 pkt_size, sprites_size, map_size;
  u32 n_sprites, x, y, i, j;
  u8 *m, b;
  struct sprite_init* s;
  struct serv_pkt_hello* pkt = buf;

  if (get_connection_state() != CONN_JOINING) {
    pkt_type_error("hello");
    return;
  }

  // size check
  map_size = (pkt->map_h * pkt->map_w) >> 2;
  n_sprites = pkt->n_sprites;
  sprites_size = n_sprites * sizeof(*s);
  pkt_size = sizeof(*pkt) + map_size + sprites_size;

  if (pkt_size != len) {
    pkt_size_error("hello", len, pkt_size);
    return; // malformed packet, drop it
  }
  len -= sizeof(*pkt);

  sprites.n = 0;

  map.w = pkt->map_w;
  map.h = pkt->map_h;
  map.tiles = malloc(map_size);

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
      else
        // init data refers to the player
        player.pos = s->transform.pos;
      len -= sizeof(*s);
      ++s;
    }
  }
  // process map data
  if (n_sprites == 0) {
    x = y = 0;
    for (i = 0; i < len; ++i) {
      b = m[i];
      for (j = 0; j < (8 >> 1); ++j) {
        map.tiles[x + y * map.w] = b & 0b11;
        b >>= 2;
        if (++x == map.w) {
          x = 0;
          ++y;
        }
      }
    }
  }

  set_connection_state(CONN_WAITING);
}

static void serv_update_handler(void* buf, u32 len) {
  struct sprite_update* u;
  struct sprite* s;
  struct serv_pkt_update* pkt = buf;

  if (get_connection_state() != CONN_UPDATING) {
    pkt_type_error("update");
    return;
  }

  if (sizeof(*pkt) != len) {
    pkt_size_error("update", len, sizeof(*pkt));
    return; // malformed packet, drop it
  }

  u = &pkt->update;
  if (u->id == player_id) {
    player.pos = u->transform.pos;
    set_player_rot(u->transform.rot);
  } else if ((s = get_sprite(u->id, false)))
    apply_sprite_transform(s, &u->transform);
}

static void serv_create_handler(void* buf, u32 len) {
  struct serv_pkt_create* pkt = buf;

  if (get_connection_state() != CONN_UPDATING &&
      get_connection_state() != CONN_WAITING) {
    pkt_type_error("create");
    return;
  }

  if (sizeof(*pkt) != len) {
    pkt_size_error("create", len, sizeof(*pkt));
    return;
  }
  init_sprite(&pkt->sprite);
}

static u32 count_player_sprites(void) {
  u32 i, n = 0;
  for (i = 0; i < sprites.n; ++i)
    n += (sprites.s[i].desc.type == SPRITE_PLAYER);
  return n;
}

static void serv_destroy_handler(void* buf, u32 len) {
  struct serv_pkt_destroy* pkt = buf;

  if (get_connection_state() != CONN_UPDATING &&
      get_connection_state() != CONN_WAITING) {
    pkt_type_error("destroy");
    return;
  }

  if (sizeof(*pkt) != len) {
    pkt_size_error("destroy", len, sizeof(*pkt));
    return;
  }

  if (pkt->desc.id == player_id) {
    switch_to_state(STATE_OVER);
    tracked_sprite = get_sprite(pkt->desc.field, false);
    return;
  }

  destroy_sprite(pkt->desc.id);
  // handle bullet points and damage
  if (pkt->desc.type == SPRITE_BULLET && pkt->desc.field != 0) {
    if (pkt->desc.owner == player_id)
      // TODO: add player reward
      puts("player score!");
    else if (pkt->desc.field == player_id)
      player.health -= BULLET_DAMAGE;
  }

  if (pkt->desc.type == SPRITE_PLAYER &&
      count_player_sprites() == 0) {
    switch_to_state(STATE_OVER);
  }
}

static void serv_wait_handler(void* buf, u32 len) {
  struct serv_pkt_wait* pkt = buf;

  if (get_connection_state() != CONN_WAITING) {
    pkt_type_error("wait");
    return;
  }

  if (sizeof(*pkt) != len) {
    pkt_size_error("wait", len, sizeof(*pkt));
    return;
  }

  if (!pkt->wait && pkt->seconds == 0) {
    set_connection_state(CONN_UPDATING);
    switch_to_state(STATE_GAME);
    game_start = time();
  }
  else
    wait_time = pkt->wait ? -1.0f : (f32)pkt->seconds;
}

static void serv_terminate_handler(void* buf, u32 len) {
  struct serv_pkt_terminate* pkt = buf;

  if (sizeof(*pkt) != len) {
    pkt_size_error("terminate", len, sizeof(*pkt));
    return;
  }

  set_connection_state(CONN_DISCONNECTED);
  if (get_client_state() != STATE_OVER)
    switch_to_state(STATE_ERROR);
}

static const serv_pkt_handler_t serv_pkt_handlers[SPKT_MAX] = {
  [SPKT_HELLO]   = serv_hello_handler,
  [SPKT_UPDATE]  = serv_update_handler,
  [SPKT_CREATE]  = serv_create_handler,
  [SPKT_DESTROY] = serv_destroy_handler,
  [SPKT_WAIT]    = serv_wait_handler,
  [SPKT_TERMINATE] = serv_terminate_handler
};

void multiplayer_on_recv(u32 len) {
  struct serv_pkt_hdr* hdr;

  if (len > sizeof(__pkt_buf)) {
    eprintf("packet too big! (max. packet size is %u bytes, but got %u)\n",
            sizeof(__pkt_buf), len);
    return;
  }

  hdr = (struct serv_pkt_hdr*)__pkt_buf;
  if (!hdr)
    return; // no message received or recv error
  if (len < sizeof(*hdr)) {
    pkt_size_error("server", len, sizeof(*hdr));
    return; // no data?
  }

  // FIXME: fix this lol
  if (hdr->seq - server_seq < MAX_PACKET_DROP)
    server_seq = hdr->seq + 1;
  else {
    eprintf("invalid sequence number (expected %u got %u)\n",
            server_seq, hdr->seq);
    return;
  }

  if (hdr->type >= SPKT_MAX) {
    eprintf("unknown packet type (got %u)\n", hdr->type);
    return; // unknown packet type, drop it
  }

  serv_pkt_handlers[hdr->type](hdr, len);
}
