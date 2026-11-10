// SPDX-License-Identifier: GPL-3.0-or-later
#include "Buttons.h"

void
Button::begin()
{
  // The buttons have external pull-ups. GPIO 34-39 have no internal pull-ups,
  // INPUT_PULLUP is harmless there.
  pinMode(pin_, INPUT_PULLUP);
  lastRaw_ = stableDown_ = digitalRead(pin_) == LOW;
  lastChangeMs_ = millis();
}

void
Button::loop()
{
  const uint32_t now = millis();
  const bool raw = digitalRead(pin_) == LOW;
  pressedEdge_ = false;
  if (raw != lastRaw_) {
    lastRaw_ = raw;
    lastChangeMs_ = now;
  }
  if (raw != stableDown_ && now - lastChangeMs_ >= debounceMs_) {
    stableDown_ = raw;
    pressedEdge_ = stableDown_;
  }
}
