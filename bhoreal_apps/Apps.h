// Every app on the Bhoreal exposes the same four entry points; the launcher
// in bhoreal_apps.ino calls them. The grid is 8x8, row 0 = the USB edge.
#pragma once
#include <Arduino.h>

struct App {
  void (*enter)();                          // app chosen in the menu
  void (*loop)();                           // called every main loop
  void (*press)(uint8_t row, uint8_t col);  // a button went down
  void (*exit)();                           // back to the menu: stop sounds etc.
};

namespace seq { extern const App app; }
namespace talps { extern const App app; }
namespace simon { extern const App app; }
namespace memory { extern const App app; }
namespace balls { extern const App app; }
namespace frogger { extern const App app; }
namespace snake { extern const App app; }
namespace connect4 { extern const App app; }
namespace duel { extern const App app; }
namespace pong { extern const App app; }

// Shared drawing helper: two 3x5 digits (00-99) in rows firstRow..firstRow+4.
void drawNumber(uint8_t firstRow, uint16_t value, uint8_t r, uint8_t g, uint8_t b);
