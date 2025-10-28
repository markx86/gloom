#include <gloom/libc.h>

extern void __heap_base;

#define BUFSZ 128

static void* g_heap_top = &__heap_base;
static u32 g_heap_size = 0;

/* We use a buffer for printf, so that we're not constantly calling write
 * everytime we need to write a character.
 */
struct printf_buf {
  u32 end;
  u32 len;
  i32 fd;
  char* buf;
};

static inline
void buf_putc(struct printf_buf* pb, char c) {
  if (pb->end >= pb->len) {
    if (pb->fd < 0)
      return;
    platform_write(pb->fd, pb->buf, pb->len);
    pb->end = 0;
  }
  pb->buf[pb->end++] = c;
}

static inline
void buf_flush(struct printf_buf* pb) {
  if (pb->end > 0 && pb->fd > 0)
    platform_write(pb->fd, pb->buf, pb->end);
}

static inline
void buf_putr(struct printf_buf* pb, const char* buf, u32 len) {
  for (; len > 0; --len)
    buf_putc(pb, buf[len-1]);
}

#define PRINTF_HANDLER(type, ...) \
  static void buf_put##type(struct printf_buf* pb, ##__VA_ARGS__)

PRINTF_HANDLER(s, const char* s) {
  while (*s)
    buf_putc(pb, *(s++));
}

static
u32 convert_u32_to_str(u32 n, char* tmp, u32 len) {
  char c;
  u32 l = 0;

  while (n > 0 && l < len) {
    c = n % 10;
    c += '0';
    tmp[l++] = c;
    n /= 10;
  }

  return l;
}

PRINTF_HANDLER(u, u32 n) {
  char tmp[32];
  u32 l;

  /* Fast path */
  if (n == 0) {
    buf_putc(pb, '0');
    return;
  }

  l = convert_u32_to_str(n, tmp, sizeof(tmp));
  buf_putr(pb, tmp, l);
}

PRINTF_HANDLER(d, i32 n) {
  if (n < 0) {
    buf_putc(pb, '-');
    n = -n;
  }
  buf_putu(pb, n);
}

PRINTF_HANDLER(x, u32 n) {
  char tmp[32], c;
  u32 l = 0;

  /* Fast path */
  if (n == 0) {
    buf_putc(pb, '0');
    return;
  }

  while (n > 0 && l < sizeof(tmp)) {
    c = n & 0xF;
    c += (c < 0xA) ? '0' : ('A' - 0xA);
    tmp[l++] = c;
    n >>= 4;
  }
  /* Pad integers with 8 zeros (not spec compliant, but idgaf) */
  while (l < 8)
    tmp[l++] = '0';

  buf_putr(pb, tmp, l);
}

PRINTF_HANDLER(f, f32 v) {
  char tmp[32];
  u32 w, d, l;

  /* Fast path */
  if (v == 0.0f) {
    buf_puts(pb, "0.0");
    return;
  }

  if (v < 0.0f) {
    v = -v;
    buf_putc(pb, '-');
  }

  w = (u32)v;
  d = (u32)((v - (f32)w) * 1e8);

  buf_putu(pb, w);
  buf_putc(pb, '.');

  l = convert_u32_to_str(d, tmp, sizeof(tmp));
  while (l < 8)
    tmp[l++] = '0';
  buf_putr(pb, tmp, l);
}

static
void process_fmt(struct printf_buf* pb, const char* fmt, va_list ap) {
  char c;

  while ((c = *(fmt++))) {
    if (c != '%') {
      buf_putc(pb, c);
      continue;
    }

    c = *(fmt++);
    switch (c) {
      case 'i':
      case 'd':
        buf_putd(pb, va_arg(ap, i32));
        break;
      case 'u':
        buf_putu(pb, va_arg(ap, u32));
        break;
      case 'x':
        buf_putx(pb, va_arg(ap, u32));
        break;
      case 's':
        buf_puts(pb, va_arg(ap, const char*));
        break;
      case 'f':
        buf_putf(pb, (f32)va_arg(ap, double));
        break;
      case 'c':
        buf_putc(pb, (char)va_arg(ap, i32));
        break;
      default:
        /* Ignore unknown specs */
        buf_putc(pb, '%');
        /* Fall through */
      case '%':
        buf_putc(pb, c);
        break;
    }
  }
}

void vsnprintf(char* buf, u32 len, const char* fmt, va_list ap) {
  struct printf_buf pb;
  pb.end = 0;
  pb.fd = -1;
  pb.len = len;
  pb.buf = buf;
  process_fmt(&pb, fmt, ap);
  if (pb.end >= pb.len)
    --pb.end;
  pb.buf[pb.end] = '\0';
}

void vfdprintf(int fd, const char* fmt, va_list ap) {
  char buf[BUFSZ];
  struct printf_buf pb;
  pb.end = 0;
  pb.fd = fd;
  pb.len = sizeof(buf);
  pb.buf = buf;
  process_fmt(&pb, fmt, ap);
  buf_flush(&pb);
}

void* malloc(u32 size) {
  void* ptr = g_heap_top;
  if (g_heap_size < size)
    g_heap_size += platform_request_mem(size);
  g_heap_top += size;
  g_heap_size -= size;
  return ptr;
}

void free_all(void) {
  g_heap_size += g_heap_top - &__heap_base;
  g_heap_top = &__heap_base;
}

void memset(void* p, u8 b, u32 l) {
  u8* sp;
  u32 bb, l4 = l >> 2, *bp = p;

  if (l4 > 0) {
    bb = (u32)b << 8 | b;
    bb |= bb << 16;
    for (; l4 > 0; --l4)
      *(bp++) = bb;
    l &= 3;
    sp = (u8*)bp;
  } else
    sp = p;

  for (; l > 0; --l)
    *(sp++) = b;
}

u32 strlen(const char* s) {
  u32 l = 0;
  while (*(s++))
    ++l;
  return l;
}
