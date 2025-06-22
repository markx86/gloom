#ifndef __STATE_GLOBALS_H__
#define __STATE_GLOBALS_H__

#include <types.h>

struct sprite;

extern const struct state_handlers
  error_state,
  loading_state,
  waiting_state,
  game_state,
  pause_state,
  options_state,
  over_state;

extern struct sprite* tracked_sprite;
extern f32 mouse_sensitivity;

void apply_settings(void);
void set_ready(b8 yes);
void set_wait_time(f32 wtime);

#endif
