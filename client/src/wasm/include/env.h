#ifndef __ENV_H__
#define __ENV_H__

#include <types.h>

extern void write(int fd, const char* s, u32 l);
extern void pointer_lock(void);
extern void pointer_release(void);
extern u32 request_mem(u32 sz);
extern void register_fb(void* fb, u32 width, u32 height, u32 size);
extern i32 send_packet(void* pkt, u32 len);

#endif
