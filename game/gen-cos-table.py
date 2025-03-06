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

with open("__cos_table.c", "w") as f:
    f.write(src)


hdr = f"""
#ifndef __MATH_H__
#error "Do not include __cos_table.h directly. Include math.h instead"
#endif

extern const f32 __cos_table[];

#define __SAMPLES         {samples}
#define __STEP            {1 / STEP}
"""

with open("__cos_table.h", "w") as f:
    f.write(hdr)
