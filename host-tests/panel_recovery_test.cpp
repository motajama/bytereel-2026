#include <cassert>
#include <cstdio>
#include "../firmware/bytereel_2026/panel_state.h"

int main() {
  InputDebouncer panel;
  assert(!panel.ready);
  panel.update(0, 0xffff, true, true, true, true, true);
  panel.update(30, 0xffff, true, true, true, true, true);
  assert(panel.ready);

  // A valid active-low HOME press is reported only after the 30 ms debounce.
  panel.update(40, (uint16_t)(0xffff & ~(1u << 8)), true, true, true, true, true);
  assert(!(panel.pressed & (1u << 8)));
  panel.update(70, (uint16_t)(0xffff & ~(1u << 8)), true, true, true, true, true);
  assert(panel.pressed & (1u << 8));

  // An I2C fault invalidates the complete sample and all debounce history.
  panel.invalidate();
  assert(!panel.ready && panel.pressed == 0 && panel.released == 0);
  const uint16_t heldHome = (uint16_t)(0xffff & ~(1u << 8));
  panel.update(100, heldHome, true, true, true, true, true);
  assert(!panel.ready && panel.pressed == 0);
  panel.update(130, heldHome, true, true, true, true, true);
  assert(panel.ready && panel.pressed == 0 && panel.released == 0);

  // Recovery cannot turn an old held state into a newly completed SAVE hold.
  HoldGesture save;
  assert(!save.update(true, 1, 0, 1000, 2000));
  save.cancel(); // firmware cancels all gestures on every failed read
  assert(!save.active && !save.done);
  assert(!save.update(true, 1, 0, 1300, 2000));
  assert(!save.update(true, 1, 0, 3299, 2000));
  assert(save.update(true, 1, 0, 3300, 2000));
  std::puts("PASS panel recovery: settle after invalid input, suppress recovery edges, restart held gestures");
}
