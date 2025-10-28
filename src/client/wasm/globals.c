#include <gloom/globals.h>
#include <gloom/game.h>

struct player g_player;
struct map g_map;
struct sprites g_sprites;
struct camera g_camera;
union keys g_keys;

struct sprite* g_tracked_sprite;
f32 g_mouse_sensitivity;
