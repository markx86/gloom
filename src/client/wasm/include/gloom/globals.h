#ifndef STATE_GLOBALS_H_
#define STATE_GLOBALS_H_

#include <gloom/types.h>

struct sprite;

extern const struct state_handlers
  g_error_state,
  g_loading_state,
  g_waiting_state,
  g_game_state,
  g_pause_state,
  g_options_state,
  g_over_state;

struct sprite* g_tracked_sprite_get(void);
void g_tracked_sprite_set(struct sprite* sprite);
void g_mouse_sensitivity_set(f32 mousesens);
void g_settings_apply(void);
void g_ready_set(b8 yes);
void g_wait_time_set(f32 wtime);

#endif
