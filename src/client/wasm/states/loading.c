#include <gloom/client.h>
#include <gloom/gloom.h>
#include <gloom/multiplayer.h>
#include <gloom/globals.h>
#include <gloom/ui.h>

#define FOREGROUND_COLOR SOLID_COLOR(LIGHTGRAY)
#define BACKGROUND_COLOR SOLID_COLOR(BLACK)
#define ERROR_COLOR      SOLID_COLOR(RED)

#define SERVER_TIMEOUT 15.0f

static
void on_back_clicked(void) {
  gloom_exit();
}

static struct component back_button = {
  .type = UICOMP_BUTTON, .text = "> back", .on_click = on_back_clicked
};

static u32 message_y;
static f32 time_in_state;
static enum multiplayer_state last_conn_state;

static
void add_message(const char* message) {
  ui_draw_string(48, message_y, message);
  message_y += STRING_HEIGHT + 8;
}

static
void on_tick(f32 delta) {
  enum multiplayer_state conn_state;

  if (time_in_state >= SERVER_TIMEOUT)
    /* The connection state has not changed in the last SERVER_TIMEOUT seconds,
     * assume something went wrong.
     */
    multiplayer_set_state(MULTIPLAYER_DISCONNECTED);

  ui_draw_component(48, FB_HEIGHT - 32 - STRING_HEIGHT, &back_button);

  conn_state = multiplayer_get_state();
  if (conn_state == last_conn_state) {
    time_in_state += delta;
    return;
  }
  last_conn_state = conn_state;
  time_in_state = 0.0f;

  /* On connection state changed */
  switch (conn_state) {
    case MULTIPLAYER_CONNECTED:
      add_message("> connected to server");
      multiplayer_join_game();
      break;

    case MULTIPLAYER_JOINING:
      add_message("> joining game");
      break;

    case MULTIPLAYER_WAITING:
      g_ready_set(false);
      client_switch_state(CLIENT_WAITING);
      break;

    /* NOTE: This should not happen, but just in case */
    case MULTIPLAYER_UPDATING:
      client_switch_state(CLIENT_GAME);
      break;

    /* Invalid state, display error */
    case MULTIPLAYER_DISCONNECTED:
      client_switch_state(CLIENT_ERROR);
      break;
  }
}

static
void on_enter(void) {
  /* If the client is not connected to the server, go to error screen */
  if (multiplayer_is_disconnected()) {
    client_switch_state(CLIENT_ERROR);
    return;
  }

  last_conn_state = MULTIPLAYER_DISCONNECTED;
  time_in_state = 0.0f;
  message_y = 32 + TITLE_HEIGHT;

  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  ui_clear_screen();
  ui_draw_title(32, 32, "loading");
  ui_on_enter(&back_button, 1);

  multiplayer_draw_game_id();
}

static
void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  ui_on_mouse_moved(x, y, dx, dy, &back_button, 1);
}

static
void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  ui_on_mouse_down(x, y, &back_button, 1);
}

static
void on_mouse_up(u32 x, u32 y, u32 button) {
  UNUSED(button);
  ui_on_mouse_up(x, y, &back_button, 1);
}

const struct state_handlers g_loading_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up
};
