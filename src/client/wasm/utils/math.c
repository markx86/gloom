#include <gloom/math.h>

#define DECLARE_TABLE
#include "cos_table.h"

f32 inv_sqrt(f32 n) {
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

f32 cos(f32 angle) {
  f32 w;
  u32 i1, i2;
  angle = absf(angle);
  angle = modf(angle, TWO_PI);
  w = angle * INV_COS_STEP;
  i1 = (u32)w;
  i2 = i1 + 1;
  if (i2 >= SAMPLES)
      i2 = 0;
  w -= (f32)i1;
  return lerp(w, g_cos_table[i1], g_cos_table[i2]);
}
