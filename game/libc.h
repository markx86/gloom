#ifndef __LIBC_H__
#define __LIBC_H__

#include <types.h>
#include <env.h>

#define UNUSED(x)         ((void)(x))
#define ARRLEN(x)         (sizeof(x) / sizeof(x[0]))
#define REINTERPRET(x, T) (*(T*)&(x))

static inline void strncpy(char* dst, const char* src, u32 len) {
  for (; *src && len > 0; --len)
    *(dst++) = *(src++);
}

static inline u32 strlen(const char* s) {
  u32 l = 0;
  while (*(s++))
    ++l;
  return l;
}

static inline void memset(void* p, u8 b, u32 l) {
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

static inline void puts(const char* s) {
  write(1, s, strlen(s));
}

void printf(const char* fmt, ...);

void* malloc(u32 size);

#endif
