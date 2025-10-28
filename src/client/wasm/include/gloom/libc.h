#ifndef LIBC_H_
#define LIBC_H_

#include <gloom/types.h>
#include <gloom/env.h>

#define UNUSED(x)         ((void)(x))
#define ARRLEN(x)         (sizeof(x) / sizeof(x[0]))
#define REINTERPRET(x, T) (*(T*)&(x))

static inline
void strncpy(char* dst, const char* src, u32 len) {
  for (; *src && len > 0; --len)
    *(dst++) = *(src++);
}

static inline
u32 strlen(const char* s) {
  u32 l = 0;
  while (*(s++))
    ++l;
  return l;
}

static inline
void strcpy(char* dst, const char* src) {
  strncpy(dst, src, strlen(src));
}

void memset(void* p, u8 b, u32 l);

static inline
void puts(const char* s) {
  platform_write(1, s, strlen(s));
}

static inline
void eputs(const char* s) {
  platform_write(2, s, strlen(s));
}

#define va_list        __builtin_va_list
#define va_start(l, p) __builtin_va_start(l, p)
#define va_end(l)      __builtin_va_end(l)
#define va_arg(l, t)   __builtin_va_arg(l, t)

void vsnprintf(char* buf, u32 len, const char* fmt, va_list ap);
void vfdprintf(int fd, const char* fmt, va_list ap);

static inline
void printf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfdprintf(1, fmt, ap);
  va_end(ap);
}

static inline
void eprintf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfdprintf(2, fmt, ap);
  va_end(ap);
}

static inline
void snprintf(char* buf, u32 len, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, len, fmt, ap);
  va_end(ap);
}

void* malloc(u32 size);
void free_all(void);

#endif
