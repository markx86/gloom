#!/usr/bin/env python3

import math
from writeutil import write_file

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

#define SAMPLES         {samples}
#define STEP            {1 / STEP}

#ifdef DECLARE_COS_TABLE

static const f32 cos_table[] = {{
    {cos_arr}
}};

#endif
"""
write_file("cos_table.h", src)
