#include <client.h>
#include <ui.h>

#define FOREGROUND_COLOR SOLIDCOLOR(LIGHTGRAY)
#define BACKGROUND_COLOR SOLIDCOLOR(BLACK)
#define ERROR_COLOR      SOLIDCOLOR(RED)

#define SERVER_TIMEOUT 15.0f

static void on_back_clicked(void) {
#ifdef UNFINISHED_FEATURES
  switch_to_state(STATE_MENU);
#else
  exit();
#endif
}

static struct component back_button = {
  .type = UICOMP_BUTTON, .text = "> back", .on_click = on_back_clicked
};

static u32 message_y;
static f32 time_in_state;
static enum connection_state last_conn_state;

static void add_message(const char* message) {
  draw_string(48, message_y, message);
  message_y += STRING_HEIGHT + 8;
}

static void on_tick(f32 delta) {
  enum connection_state conn_state;

  if (time_in_state >= SERVER_TIMEOUT)
    // the connection state has not changed in the last SERVER_TIMEOUT seconds,
    // assume something went wrong
    set_connection_state(CONN_DISCONNECTED);

  draw_component(48, FB_HEIGHT - 32 - STRING_HEIGHT, &back_button);

  conn_state = get_connection_state();
  if (conn_state == last_conn_state) {
    time_in_state += delta;
    return;
  }
  last_conn_state = conn_state;
  time_in_state = 0.0f;

  // on connection state changed
  switch (conn_state) {
    case CONN_CONNECTED:
      add_message("> sending join request");
      join_game();
      break;

    case CONN_JOINING:
      add_message("> joining game");
      break;

    case CONN_WAITING:
      switch_to_state(STATE_WAITING);
      break;

    // NOTE: this should not happen, but just in case
    case CONN_UPDATING:
      switch_to_state(STATE_GAME);
      break;

    // invalid state, display error
    case CONN_DISCONNECTED:
      switch_to_state(STATE_ERROR);
      break;
  }
}

static void on_enter(void) {
  // if the client is not connected to the server, go back to main menu
  if (is_disconnected()) {
    switch_to_state(STATE_ERROR);
    return;
  }

  last_conn_state = CONN_DISCONNECTED;
  time_in_state = 0.0f;
  message_y = 32 + TITLE_HEIGHT;

  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  clear_screen();
  draw_title(32, 32, "loading");
  component_on_enter(&back_button, 1);

  display_game_id();
}

static void on_mouse_moved(u32 x, u32 y, i32 dx, i32 dy) {
  component_on_mouse_moved(x, y, dx, dy, &back_button, 1);
}

static void on_mouse_down(u32 x, u32 y, u32 button) {
  UNUSED(button);
  component_on_mouse_down(x, y, &back_button, 1);
}

static void on_mouse_up(u32 x, u32 y, u32 button) {
  UNUSED(button);
  component_on_mouse_up(x, y, &back_button, 1);
}

const struct state_handlers loading_state = {
  .on_enter = on_enter,
  .on_tick = on_tick,
  .on_mouse_moved = on_mouse_moved,
  .on_mouse_down = on_mouse_down,
  .on_mouse_up = on_mouse_up
};
