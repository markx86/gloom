#ifndef STATE_GLOBALS_H_
#define STATE_GLOBALS_H_

#include <gloom/types.h>

/* This file contains all public global variables and functions
 * NOTE: It does not contain the global variables used in inline functions
 *       as they're not supposed to be used directly and are only exposed
 *       globally to make the code compile.
 */

struct sprite;

extern const struct state_handlers
  g_error_state,
  g_loading_state,
  g_waiting_state,
  g_game_state,
  g_pause_state,
  g_options_state,
  g_over_state;

extern struct player g_player;
extern struct camera g_camera;
extern struct sprites g_sprites;
extern struct map g_map;
extern union keys g_keys;

extern struct sprite* g_tracked_sprite;
extern f32 g_mouse_sensitivity;

void g_settings_apply(void);
void set_ready(b8 yes);
void g_wait_time_set(f32 wtime);

#endif
