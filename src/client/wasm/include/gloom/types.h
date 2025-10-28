#ifndef TYPES_H_
#define TYPES_H_

#define PACKED __attribute__((packed))

#define _STATIC_ASSERT(x) _Static_assert(x, #x)

typedef unsigned int u32;
_STATIC_ASSERT(sizeof(u32) == 4);
typedef /******/ int i32;
_STATIC_ASSERT(sizeof(i32) == 4);

typedef unsigned short u16;
_STATIC_ASSERT(sizeof(u16) == 2);
typedef /******/ short i16;
_STATIC_ASSERT(sizeof(i16) == 2);

typedef unsigned char u8;
_STATIC_ASSERT(sizeof(u8) == 1);
typedef /******/ char i8;
_STATIC_ASSERT(sizeof(i8) == 1);

typedef float f32;
_STATIC_ASSERT(sizeof(f32) == 4);

typedef unsigned char b8;
_STATIC_ASSERT(sizeof(b8) == 1);
#define true  1
#define false 0

typedef struct { i32 x, y; } PACKED vec2i;
typedef struct { u32 x, y; } PACKED vec2u;
typedef struct { f32 x, y; } PACKED vec2f;

#undef _STATIC_ASSERT

#define NULL ((void*)0)

#endif
