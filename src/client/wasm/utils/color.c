#include <gloom/color.h>

u32 _color_alpha_mask;

const u32 _color_palette[] = {
  [COLOR_BLACK]       = 0x000000,
  [COLOR_GRAY]        = 0x7E7E7E,
  [COLOR_LIGHTGRAY]   = 0xBEBEBE,
  [COLOR_WHITE]       = 0xFFFFFF,
  [COLOR_DARKRED]     = 0x00007E,
  [COLOR_RED]         = 0x0000FE,
  [COLOR_DARKGREEN]   = 0x007E04,
  [COLOR_GREEN]       = 0x04FF06,
  [COLOR_DARKYELLOW]  = 0x007E7E,
  [COLOR_YELLOW]      = 0x04FFFF,
  [COLOR_DARKBLUE]    = 0x7E0000,
  [COLOR_BLUE]        = 0xFF0000,
  [COLOR_DARKMAGENTA] = 0x7E007E,
  [COLOR_MAGENTA]     = 0xFF00FE,
  [COLOR_DARKCYAN]    = 0x7E7E04,
  [COLOR_CYAN]        = 0xFFFF06,
};
