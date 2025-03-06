#include <client.h>
#include <ui.h>

#define FOREGROUND_COLOR 0xFFAAAAAA
#define BACKGROUND_COLOR 0xFF000000

static void on_back_clicked(void) { switch_to_state(STATE_MENU); }

static struct component back_button = {
  .type = UICOMP_BUTTON, .text = "> back", .on_click = on_back_clicked
};

static u32 message_y;
static enum connection_state last_conn_state;

static void add_message(const char* message) {
  draw_string(48, message_y, message);
  message_y += STRING_HEIGHT;
}

static void on_tick(f32 delta) {
  enum connection_state conn_state;
  UNUSED(delta);

  multiplayer_tick();

  conn_state = get_connection_state();
  if (conn_state == last_conn_state)
    return;

  // on connection state changed
  switch (conn_state) {
    case CONN_CONNECTED:
      join_game(0xdeadbeef, 0xcafebabe);
      break;

    case CONN_JOINING:
      add_message("> joining game");
      break;

    case CONN_UPDATING:
      switch_to_state(STATE_GAME);
      break;

    case CONN_LEAVING:
      break;

    // invalid state, display error
    case CONN_UNKNOWN:
    case CONN_DISCONNECTED:
      add_message("> an error has occurred");
      draw_component(48, message_y, &back_button);
      break;
    }

  last_conn_state = conn_state;
}

static void on_enter(enum client_state prev_state) {
  UNUSED(prev_state);

  // if the client is not connected to the server, go back to main menu
  if (is_disconnected()) {
    switch_to_state(STATE_MENU);
    return;
  }

  last_conn_state = CONN_UNKNOWN;
  message_y = 32 + TITLE_HEIGHT;

  set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);

  clear_screen();

  draw_title(32, 32, "loading");
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
