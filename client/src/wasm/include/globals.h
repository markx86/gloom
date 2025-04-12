#ifndef __STATE_GLOBALS_H__
#define __STATE_GLOBALS_H__

struct sprite;

extern const struct state_handlers
  menu_state,
  loading_state,
  game_state,
  pause_state,
  options_state,
  about_state,
  dead_state;

extern struct sprite* tracked_sprite;

#endif
