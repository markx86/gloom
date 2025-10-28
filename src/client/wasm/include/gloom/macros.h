#ifndef MACROS_H_
#define MACROS_H_

#define UNUSED(x)         ((void)(x))
#define ARRLEN(x)         (sizeof(x) / sizeof(x[0]))

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#endif
