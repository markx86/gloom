#ifndef __MATH_H__
#define __MATH_H__

#include <types.h>

#define SQRT2     1.4142135623730951f
#define INV_SQRT2 0.7071067811865475f

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/*
static inline f32 inv_sqrt(f32 n) {
  i32 i;
  f32 x2, y;
  const f32 three_halfs = 1.5f;
  x2 = n * 0.5f;
  y = n;
  i = *(i32*)&y;
  i = 0x5F3759DF - (i >> 1);
  y = *(f32*)&i;
  y = y * (three_halfs - ( x2 * y * y ) );
  return y;
}
*/

static inline i32 isposf(f32 x) {
  u32 v = *(u32*)&x;
  return !(v & (1U << 31));
}

static inline f32 signf(f32 x) {
  return (isposf(x) << 1) - 1;
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

static inline u32 abs(i32 v) {
  return (v > 0) ? v : -v;
}

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

#define PI         3.141592653589793f
#define TWO_PI     (PI * 2.0f)
#define HALF_PI    (PI / 2.0f)
#define QUARTER_PI (HALF_PI / 2.0f)

#include <__cos_table.h>

static inline f32 cos(f32 angle) {
    f32 w;
    u32 i1, i2;
    angle = absf(angle);
    angle = modf(angle, TWO_PI);
    w = angle * __STEP;
    i1 = (u32)w;
    i2 = i1 + 1;
    if (i2 >= __SAMPLES)
        i2 = 0;
    w -= (f32)i1;
    return lerp(w, __cos_table[i1], __cos_table[i2]);
}

static inline f32 sin(f32 angle) {
    return cos(angle - HALF_PI);
}

static inline f32 tan(f32 angle) {
    return sin(angle) / cos(angle);
}

#define DEG2RAD(x) ((x) * (PI / 180.0f))

#endif
