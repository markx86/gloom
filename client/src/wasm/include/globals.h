#ifndef __STATE_GLOBALS_H__
#define __STATE_GLOBALS_H__

#include <types.h>
struct sprite;

extern const struct state_handlers
  error_state,
  menu_state,
  loading_state,
  waiting_state,
  game_state,
  pause_state,
  options_state,
  about_state,
  dead_state;

extern struct sprite* tracked_sprite;
extern f32 wait_time;

#endif
