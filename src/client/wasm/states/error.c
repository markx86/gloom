#include <gloom/client.h>
#include <gloom/ui.h>

#define BACKGROUND_COLOR SOLID_COLOR(RED)
#define FOREGROUND_COLOR SOLID_COLOR(WHITE)

static
void on_enter(void) {
  ui_set_colors(FOREGROUND_COLOR, BACKGROUND_COLOR);
  ui_clear_screen();

  ui_draw_title(32, 32, "disconnected");
  ui_draw_string(48, 32 + TITLE_HEIGHT,
                 "a fatal error occurred, you've been disconnected");
}

const struct state_handlers g_error_state = {
  .on_enter = on_enter
};
