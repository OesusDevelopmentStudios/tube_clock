// SPDX-License-Identifier: GPL-3.0-or-later
#include "ChipSelect.h"

void
ChipSelect::begin()
{
  pinMode(CSSR_LATCH_PIN, OUTPUT);
  pinMode(CSSR_DATA_PIN, OUTPUT);
  pinMode(CSSR_CLOCK_PIN, OUTPUT);
  digitalWrite(CSSR_DATA_PIN, LOW);
  digitalWrite(CSSR_CLOCK_PIN, LOW);
  digitalWrite(CSSR_LATCH_PIN, LOW);
  update();
}

void
ChipSelect::setDigit(uint8_t digit)
{
  digitsMap_ = uint8_t(1u << digit);
  update();
}

void
ChipSelect::setAll()
{
  digitsMap_ = 0x3F;
  update();
}

void
ChipSelect::clear()
{
  digitsMap_ = 0;
  update();
}

void
ChipSelect::update()
{
  // Q7 and Q6 are unused; Q5 is digit 0 ... Q0 is digit 5. Q7 is shifted out first,
  // so two dummy bits go first. CS is active low, the map is active high -> invert.
  const uint8_t toShift = uint8_t((~digitsMap_) << 2);
  digitalWrite(CSSR_LATCH_PIN, LOW);
  shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, LSBFIRST, toShift);
  digitalWrite(CSSR_LATCH_PIN, HIGH);
}
