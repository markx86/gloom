#ifndef __ENV_H__
#define __ENV_H__

#include <types.h>

extern void write(int fd, const char* s, u32 l);
extern void pointer_lock(void);
extern void pointer_release(void);

#endif
