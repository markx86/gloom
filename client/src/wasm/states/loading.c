#include <client.h>
#include <ui.h>

#define FOREGROUND_COLOR solid_color(LIGHTGRAY)
#define BACKGROUND_COLOR solid_color(BLACK)
#define ERROR_COLOR      solid_color(RED)

static void on_back_clicked(void) { switch_to_state(STATE_MENU); }

static struct component back_button = {
  .type = UICOMP_BUTTON, .text = "> back", .on_click = on_back_clicked
};

static u32 message_y;
static f32 time_in_state;
static enum connection_state last_conn_state;

static void add_message(const char* message) {
  draw_string(48, message_y, message);
  message_y += 24;
}

static void on_tick(f32 delta) {
  enum connection_state conn_state;

  if (time_in_state >= 10.0f)
    // the connection state has not changed in the last 10s, assume something
    // went wrong (this is not the cleanest way to do this, but bad design
    // forced my hand!)
    set_connection_state(CONN_UNKNOWN);

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
      if (!join_game(0xcafebabe))
        conn_state = CONN_UNKNOWN;
      break;

    case CONN_JOINING:
      add_message("> joining game");
      break;

    case CONN_UPDATING:
      switch_to_state(STATE_GAME);
      break;

    // invalid state, display error
    case CONN_UNKNOWN:
    case CONN_DISCONNECTED:
      set_colors(ERROR_COLOR, BACKGROUND_COLOR);
      add_message("> an error has occurred");
      set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
      break;
  }
}

static void on_enter(enum client_state prev_state) {
  UNUSED(prev_state);

  // if the client is not connected to the server, go back to main menu
  if (is_disconnected()) {
    switch_to_state(STATE_MENU);
    return;
  }

  last_conn_state = CONN_UNKNOWN;
  time_in_state = 0.0f;
  message_y = 32 + TITLE_HEIGHT;

  set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  clear_screen();
  draw_title(32, 32, "loading");
  component_on_enter(&back_button, 1);
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
