#!/usr/bin/env python3

import math

STEP = 0.005
RANGE = 2 * math.pi

samples = int(RANGE // STEP) + 1
table_size = samples * 4
print(f"predicted cosine table size = {round(table_size / 1024)}KB")

cos_table = []
for i in range(samples):
    a = STEP * i
    cos_table.append(math.cos(a))

cos_arr = "f,".join(str(v) for v in cos_table)

src = f"""#include <types.h>

const f32 __cos_table[] = {{
    {cos_arr}
}};
"""

with open("trigo.c", "w") as f:
    f.write(src)


hdr = f"""#ifndef __TRIGO_H__
#define __TRIGO_H__

#ifndef __MATH_H__
#error "Do not include trigo.h directly. Include math.h instead"
#endif

#define PI         {math.pi}f
#define TWO_PI     {2 * math.pi}f
#define HALF_PI    {math.pi / 2}f
#define QUARTER_PI {math.pi / 4}f

extern const f32 __cos_table[];

#define __SAMPLES         {samples}
#define __STEP            {1 / STEP}

static inline f32 __probe_table(const f32* table, f32 angle) {{
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
    return lerp(w, table[i1], table[i2]);
}}

#undef __ABSD
#undef __MODD

static inline f32 cos(f32 angle) {{
    return __probe_table(__cos_table, angle);
}}

static inline f32 sin(f32 angle) {{
    return cos(angle - HALF_PI);
}}

static inline f32 tan(f32 angle) {{
    return sin(angle) / cos(angle);
}}

#endif
"""

with open("trigo.h", "w") as f:
    f.write(hdr)
