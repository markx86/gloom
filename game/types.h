#ifndef __TYPES_H__
#define __TYPES_H__

#define __STATICASSERT(x) _Static_assert(x, #x)

typedef unsigned int u32;
__STATICASSERT(sizeof(u32) == 4);
typedef /******/ int i32;
__STATICASSERT(sizeof(i32) == 4);

typedef unsigned short u16;
__STATICASSERT(sizeof(u16) == 2);
typedef /******/ short i16;
__STATICASSERT(sizeof(i16) == 2);

typedef unsigned char u8;
__STATICASSERT(sizeof(u8) == 1);
typedef /******/ char i8;
__STATICASSERT(sizeof(i8) == 1);

typedef float f32;
__STATICASSERT(sizeof(f32) == 4);

typedef unsigned char b8;
__STATICASSERT(sizeof(b8) == 1);
#define true  1
#define false 0

typedef struct { i32 x, y; } vec2i;
typedef struct { u32 x, y; } vec2u;
typedef struct { f32 x, y; } vec2f;

#undef __STATICASSERT

#endif
