// SPDX-License-Identifier: GPL-3.0-or-later
// Polled button state tracking. Based on EleksTubeHAX by SmittyHalibut.
#pragma once

#include <Arduino.h>

#include "GLOBAL_DEFINES.h"

class Button
{
public:
  explicit Button(uint8_t pin, uint32_t debounceMs = 30)
    : pin_(pin)
    , debounceMs_(debounceMs)
  {
  }

  void begin();
  void loop();

  // True exactly once per press (debounced falling edge).
  bool pressed() const { return pressedEdge_; }
  bool isDown() const { return stableDown_; }

private:
  const uint8_t pin_;
  const uint32_t debounceMs_;
  bool stableDown_ = false;
  bool lastRaw_ = false;
  bool pressedEdge_ = false;
  uint32_t lastChangeMs_ = 0;
};

struct Buttons
{
  Button left{ BUTTON_LEFT_PIN };
  Button mode{ BUTTON_MODE_PIN };
  Button right{ BUTTON_RIGHT_PIN };
  Button power{ BUTTON_POWER_PIN };

  void begin()
  {
    left.begin();
    mode.begin();
    right.begin();
    power.begin();
  }
  void loop()
  {
    left.loop();
    mode.loop();
    right.loop();
    power.loop();
  }
};
