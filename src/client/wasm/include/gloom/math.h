#ifndef MATH_H_
#define MATH_H_

#include <gloom/platform.h>

#define SQRT2     1.4142135623730951f
#define INV_SQRT2 0.7071067811865475f

f32 inv_sqrt(f32 n);

static inline
i32 roundf(f32 x) {
  const i32 i = (i32)x;
  return (x - (f32)i < 0.5) ? i : i+1;
}

static inline
i32 isposf(f32 x) {
  u32 v = *(u32*)&x;
  return !(v & (1U << 31));
}

static inline
f32 signf(f32 x) {
  return (isposf(x) << 1) - 1;
}

static inline
f32 lerp(f32 weight, f32 v1, f32 v2) {
  return (1.0f - weight) * v1 + weight * v2;
}

static inline
f32 absf(f32 f) {
  u32 a = (*(u32*)&f) & ~(1U << 31);
  return *(f32*)&a;
}

static inline
f32 modf(f32 val, f32 mod) {
  return val - (i32)(val / mod) * mod;
}

static inline
u32 abs(i32 v) {
  return (v > 0) ? v : -v;
}

#define VEC2ADD(v, w)     \
  ((typeof(*(v))) {       \
    .x = (v)->x + (w)->x, \
    .y = (v)->y + (w)->y  \
  })

#define VEC2SUB(v, w)     \
  ((typeof(*(v))) {       \
    .x = (v)->x - (w)->x, \
    .y = (v)->y - (w)->y  \
  })

#define VEC2SCALE(v, s)  \
  ((typeof(*(v))) {      \
    .x = (v)->x * s,     \
    .y = (v)->y * s      \
  })

#define VEC2LENGTH2(v) \
  ((v)->x * (v)->x + (v)->y * (v)->y)

#define PI         3.141592653589793f
#define TWO_PI     (PI * 2.0f)
#define HALF_PI    (PI / 2.0f)
#define QUARTER_PI (HALF_PI / 2.0f)

static inline
vec2f vec2f_normalized(vec2f* vec) {
  vec2f v;
  f32 mod = inv_sqrt(VEC2LENGTH2(vec));
  v.x = vec->x * mod;
  v.y = vec->y * mod;
  return v;
}

f32 cos(f32 angle);

static inline
f32 acos(f32 value) {
  return platform_acos(value);
}

static inline
f32 sin(f32 angle) {
    return cos(angle - HALF_PI);
}

static inline
f32 tan(f32 angle) {
    return sin(angle) / cos(angle);
}

#define DEG2RAD(x) ((x) * (PI / 180.0f))

#endif
