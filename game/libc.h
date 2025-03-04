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

static inline void puts(const char* s) {
  write(1, s, strlen(s));
}

void printf(const char* fmt, ...);

void* malloc(u32 size);

#endif
