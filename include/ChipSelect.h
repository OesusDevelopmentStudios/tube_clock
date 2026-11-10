// SPDX-License-Identifier: GPL-3.0-or-later
// Chip select of the six displays through the 74HC595 shift register.
// Based on EleksTubeHAX by SmittyHalibut / aly-fly.
#pragma once

#include "GLOBAL_DEFINES.h"

class ChipSelect
{
public:
  void begin();
  // Physical digit index (see SCREEN_TO_DIGIT): selects exactly this display.
  void setDigit(uint8_t digit);
  void setAll();
  void clear();

private:
  void update();
  uint8_t digitsMap_ = 0; // bit n = display n selected
};
