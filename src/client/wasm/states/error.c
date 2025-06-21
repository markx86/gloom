#include <client.h>
#include <ui.h>

#define BACKGROUND_COLOR SOLIDCOLOR(RED)
#define FOREGROUND_COLOR SOLIDCOLOR(WHITE)

static void on_enter(void) {
  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  clear_screen();

  draw_title(32, 32, "disconnected");
  draw_string(48, 32 + TITLE_HEIGHT,
              "a fatal error occurred, you've been disconnected");
}

const struct state_handlers error_state = {
  .on_enter = on_enter
};
