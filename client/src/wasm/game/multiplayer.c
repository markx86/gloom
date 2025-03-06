#include <gloom.h>

#define PACKED       __attribute__((packed))
#define WS_PORT      8492
#define PKT_BUF_SIZE 256

#define MAX_PLAYERS     16
#define MAX_PACKET_DROP 5

enum game_pkt_type {
  GPKT_JOIN,
  GPKT_LEAVE,
  GPKT_UPDATE,
  GPKT_MAX
};

struct game_pkt_hdr {
  u32 seq  : 30;
  u32 type : 2;
  u32 player_token;
} PACKED;

struct game_pkt_join {
  struct game_pkt_hdr hdr;
  u32 game_id;
} PACKED;

struct game_pkt_leave {
  struct game_pkt_hdr hdr;
};

struct game_pkt_update {
  struct game_pkt_hdr hdr;
  vec2f pos;
  f32 rot;
  u32 keys;
} PACKED;

enum serv_pkt_type {
  SPKT_HELLO,
  SPKT_UPDATE,
  SPKT_BYE,
  SPKT_MAX
};

struct serv_pkt_hdr {
  u32 seq  : 30;
  u32 type : 2;
} PACKED;

struct serv_hello_pkt {
  struct serv_pkt_hdr hdr;
  u32 n_sprites : 24;
  u32 player_id : 8;
  u32 map_w, map_h;
  char data[0];
} PACKED;

struct sprite_init {
  u32 color : 24;
  u32 id    : 8;
  vec2f pos;
} PACKED;

struct sprite_update {
  u8 id;
  f32 rot;
  vec2f pos;
  vec2f vel;
} PACKED;

struct serv_update_pkt {
  struct serv_pkt_hdr hdr;
  struct sprite_update update;
} PACKED;

static u8 player_id;
static u32 player_token, seq;

static char __pkt_buf[PKT_BUF_SIZE];
enum connection_state __conn_state = CONN_UNKNOWN;

typedef void (*serv_pkt_handler_t)(void*, u32);

static void init_game_pkt(void* hdrp, enum game_pkt_type type) {
  struct game_pkt_hdr* hdr = hdrp;
  hdr->type = type;
  hdr->seq = seq++;
  hdr->player_token = player_token;
}

b8 join_game(u32 token, u32 game_id) {
  struct game_pkt_join pkt;
  i32 rc;

  if (__conn_state != CONN_CONNECTED)
    return false;

  player_token = token;
  seq = 0; // reset game packet sequence

  init_game_pkt(&pkt, GPKT_JOIN);
  pkt.game_id = game_id;

  rc = send_packet(&pkt, sizeof(pkt));

  if (rc == sizeof(pkt)) {
    __conn_state = CONN_JOINING;
    return true;
  } else
    return false;
}

b8 leave_game(void) {
  struct game_pkt_leave pkt;
  i32 rc;
  __conn_state = CONN_LEAVING;
  init_game_pkt(&pkt, GPKT_LEAVE);
  rc = send_packet(&pkt, sizeof(pkt));
  return rc == sizeof(pkt);
}

void send_update(void) {
  struct game_pkt_update pkt;
  if (__conn_state != CONN_UPDATING)
    return;
  init_game_pkt(&pkt, GPKT_UPDATE);
  pkt.pos = player.pos;
  pkt.rot = player.rot;
  pkt.keys = keys.all_keys;
  send_packet(&pkt, sizeof(pkt));
}

static void* next_packet(u32* len) {
  i32 rc;
  u32 cpy, n_cpy, i;
  if (*len) {
    cpy = sizeof(__pkt_buf) - *len;
    n_cpy = *len;
    i = 0;
    for (; n_cpy > 0; --n_cpy)
      __pkt_buf[i] = __pkt_buf[cpy + i];
  }
  rc = recv_packet(__pkt_buf + *len, sizeof(__pkt_buf) - *len);
  if (rc < 0) {
    eputs("packet recv error");
    __conn_state = CONN_UNKNOWN; // something happened, drop the connection
    return NULL;
  }
  *len = rc;
  return rc == 0 ? NULL : __pkt_buf;
}

static void pkt_size_error(const char* pkt_type, u32 got, u32 expected) {
  eprintf("%s packet size is not what was expected (should be %u, got %u)\n", pkt_type, expected, got);
}

static void pkt_type_error(const char* pkt_type) {
  eprintf("received %s packet, but the connection state is wrong (now in %d)\n", pkt_type, __conn_state);
}

static inline struct sprite* get_sprite_by_id(u8 sid) {
  u32 id = sid < player_id ? sid : sid - 1;
  if (id >= sprites.n) {
    eprintf("invalid sprite with id %u (max. id is %u)\n", sid, sprites.n + 1);
    return NULL;
  }
  return sprites.arr + id;
}

static void serv_hello_handler(void* buf, u32 len) {
  void* data;
  u32 pkt_size, sprites_size, map_size;
  u32 n_sprites, x, y, i, off;
  u8 *m, b;
  struct sprite_init* s;
  struct sprite* sprite;
  struct serv_hello_pkt* pkt = buf;

  if (__conn_state != CONN_JOINING) {
    pkt_type_error("hello");
    return;
  }

  if (pkt->hdr.seq != 1)
    printf("hello packet from server, has sequence number %u (expected 1)\n", pkt->hdr.seq);

  // size check
  map_size = (pkt->map_h * pkt->map_w) >> 2;
  n_sprites = pkt->n_sprites;
  sprites_size = n_sprites * sizeof(*s);
  pkt_size = sizeof(*pkt) + map_size + sprites_size;

  if (len < PKT_BUF_SIZE && pkt_size != len) {
size_err:
    pkt_size_error("hello", len, pkt_size);
    return; // malformed packet, drop it
  }

  data = pkt->data;

  sprites.n = n_sprites - 1;
  sprites.arr = malloc(sprites.n * sizeof(*s));

  map.w = pkt->map_w;
  map.h = pkt->map_h;
  map.tiles = malloc(map_size);

  player_id = pkt->player_id;

  x = y = i = 0;

  for (;;) {
    pkt_size -= len;

    s = data;
    if (n_sprites > 0) {
      for (; n_sprites > 0; --n_sprites) {
        if (len < sizeof(*s))
          break;
        if (s->id == player_id) {
          // init data refers to the player
          player.pos = s->pos;
        } else {
          sprite = get_sprite_by_id(s->id);
          if (sprite) {
            *sprite = (struct sprite) {
              .color = 0xFF000000 | s->color,
              .pos = s->pos,
              .dim = {
                .x = PLAYER_SPRITE_W,
                .y = PLAYER_SPRITE_H
              }
            };
          }
        }
        len -= sizeof(*s);
        ++s;
      }
    }
    if (n_sprites == 0) {
      m = (u8*)s;
      b = m[i++];
      for (; y < map.h; ++y) {
        for (; x < map.w; ++x) {
          off = x + y * map.w;
          map.tiles[x + y * map.w] = b & 0b11;
          b >>= 2;
          if ((off & 3) == 3) {
            b = m[i++];
            if (--len == 0) {
              if (++x == map.w) {
                x = 0;
                ++y;
              }
              goto next;
            }
          }
        }
        x = 0;
      }
    }

next:
    if (pkt_size == 0)
      break; // we done, exit the loop

    data = next_packet(&len);
    if (!data)
      return; // recv error
    if (len < PKT_BUF_SIZE && len != pkt_size)
      goto size_err;
  }

  __conn_state = CONN_UPDATING;
}

static void serv_update_handler(void* buf, u32 len) {
  struct sprite_update* u;
  struct sprite* s;
  struct serv_update_pkt* pkt = buf;

  if (__conn_state != CONN_UPDATING) {
    pkt_type_error("update");
    return;
  }

  if (len < PKT_BUF_SIZE && sizeof(*pkt) != len) {
    pkt_size_error("update", len, sizeof(*pkt));
    return; // malformed packet, drop it
  }

  u = &pkt->update;
  if (u->id == player_id) {
    player.pos = u->pos;
    set_player_rot(u->rot);
  } else if ((s = get_sprite_by_id(u->id))) {
    s->pos = u->pos;
    s->vel = u->vel;
    s->rot = u->rot;
  }

  /*
  n = pkt->n_updates;
  u = pkt->updates;

  pkt_size = sizeof(*pkt) + n * sizeof(*u);

  if (len < PKT_BUF_SIZE && pkt_size != len) {
size_err:
    pkt_size_error("update", len, pkt_size);
    return; // malformed packet, drop it
  }

  for (;;) {
    pkt_size -= len;

    for (; n > 0; --n) {
      if (len < sizeof(*u))
        break;
      s = get_sprite_by_id(u->id);
      if (s) {
        s->pos = u->pos;
        s->vel = u->vel;
        s->rot = u->rot;
      }
      len -= sizeof(*u);
      ++u;
    }

    if (pkt_size == 0)
      break; // we done, exit the loop

    u = next_packet(&len);
    if (!u)
      return; // recv error
    if (len < PKT_BUF_SIZE && len != pkt_size)
      goto size_err;
  }
  */
}

static void serv_bye_handler(void* buf, u32 len) {
  UNUSED(buf);
  UNUSED(len);

  if (__conn_state == CONN_LEAVING) {
    pkt_type_error("bye");
    return;
  }

  puts("unhandled bye packet");
}

static const serv_pkt_handler_t serv_pkt_handlers[SPKT_MAX] = {
  [SPKT_HELLO] = serv_hello_handler,
  [SPKT_UPDATE] = serv_update_handler,
  [SPKT_BYE] = serv_bye_handler
};

void multiplayer_tick(void) {
  u32 len, drops;
  struct serv_pkt_hdr* hdr;

  if (is_disconnected())
    return;

  len = 0;
  hdr = next_packet(&len);
  if (!hdr)
    return; // no message received or recv error
  if (len < sizeof(*hdr)) {
    pkt_size_error("server", len, sizeof(*hdr));
    return; // no data?
  }

  drops = abs(hdr->seq - seq);
  if (drops > MAX_PACKET_DROP) {
    eprintf("invalid sequence number");
    return;
  }
  else if (seq <= hdr->seq)
    seq = hdr->seq + 1;

  if (hdr->type >= SPKT_MAX) {
    eprintf("unknown packet type (got %u)\n", hdr->type);
    return; // unknown packet type, drop it
  }

  serv_pkt_handlers[hdr->type](hdr, len);
}
