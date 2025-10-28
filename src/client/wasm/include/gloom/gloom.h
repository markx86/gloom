#ifndef GLOOM_H_
#define GLOOM_H_

#include <gloom/types.h>

void gloom_settings_load(f32 drawdist, f32 fov, f32 mousesens, b8 camsmooth);
void gloom_settings_defaults(void);

void gloom_set_pointer_locked(b8 locked);

void gloom_on_ws_close(void);
void gloom_on_recv_packet(u32 len);
void gloom_on_key_event(u32 code, char ch, b8 pressed);
void gloom_on_mouse_down(u32 x, u32 y, u32 button);
void gloom_on_mouse_up(u32 x, u32 y, u32 button);
void gloom_on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy);

b8   gloom_tick(f32 delta);
void gloom_init(b8 ws_connected, u32 game_id, u32 player_token);
void gloom_exit(void);

void* gloom_packet_buffer(void);
u32   gloom_packet_buffer_size(void);

void* gloom_framebuffer(void);
u32   gloom_framebuffer_width(void);
u32   gloom_framebuffer_height(void);
u32   gloom_framebuffer_size(void);

#endif
