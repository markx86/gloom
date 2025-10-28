#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <gloom/types.h>

extern void platform_write(int fd, const char* s, u32 l);
extern void platform_pointer_lock(void);
extern void platform_pointer_release(void);
extern u32  platform_request_mem(u32 sz);
extern i32  platform_send_packet(void* pkt, u32 len);
extern void platform_settings_store(f32 drawdist, f32 fov, f32 mousesens, b8 camsmooth);
extern f32  platform_get_time(void);
extern f32  platform_acos(f32 value);

#endif
