#ifndef __MATH_H__
#define __MATH_H__

#include <types.h>

#define SQRT2    1.4142135623730951f
#define INVSQRT2 0.7071067811865475f

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

static inline f32 inv_sqrt(f32 n) {
  i32 i;
  f32 x2, y;
  const f32 three_halfs = 1.5f;
  x2 = n * 0.5f;
  y = n;
  i = *(i32*)&y;
  i = 0x5f3759df - (i >> 1);
  y = *(f32*)&i;
  y = y * (three_halfs - ( x2 * y * y ) );
  return y;
}

static inline f32 lerp(f32 weight, f32 v1, f32 v2) {
  return (1.0f - weight) * v1 + weight * v2;
}

static inline f32 modf(f32 val, f32 mod) {
  return (val - (i32)(val / mod) * mod);
}

static inline f32 absf(f32 f) {
  u32 a = (*(u32*)&f) & ~(1U << 31);
  return *(f32*)&a;
}

static inline f32 abs(f32 v) {
  return (v > 0) ? v : -v;
}

#define SIGN(x) ((x) < 0 ? -1 : +1)

#define VEC2ADD(v, w) \
  ((typeof(v)) {      \
    .x = v.x + w.x,   \
    .y = v.y + w.y    \
  })

#define VEC2SUB(v, w) \
  ((typeof(v)) {      \
    .x = v.x - w.x,   \
    .y = v.y - w.y    \
  })

#define VEC2SCALE(v, s) \
  ((typeof(v)) {        \
    .x = v.x * s,       \
    .y = v.y * s        \
  })

#include <trigo.h>

#define DEG2RAD(x) ((x) * (PI / 180.0f))

#endif
