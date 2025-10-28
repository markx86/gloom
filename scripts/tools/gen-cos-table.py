#!/usr/bin/env python3

import math
from writeutil import write_file

STEP = 0.005
RANGE = 2 * math.pi

samples = int(RANGE // STEP) + 1

cos_table = []
for i in range(samples):
    a = STEP * i
    cos_table.append(math.cos(a))
cos_arr = "f,".join(str(v) for v in cos_table)

src = f"""#include <gloom/types.h>

#define SAMPLES         {samples}
#define INV_COS_STEP    {1 / STEP}

#ifdef DECLARE_TABLE

static const f32 g_cos_table[] = {{
  {cos_arr}
}};

#endif // DECLARE_TABLE
"""
write_file("cos_table.h", src)
