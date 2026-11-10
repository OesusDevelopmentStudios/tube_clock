// SPDX-License-Identifier: GPL-3.0-or-later
#include "Backlights.h"

#include <math.h>

using clockcore::Pattern;

void
Backlights::begin(clockcore::Config::Backlight* config)
{
  Adafruit_NeoPixel::begin();
  config_ = config;
  off_ = false;
  needsInit_ = true;
}

uint8_t
Backlights::brightness255() const
{
  // intensity 0..7 -> 0, 2, 4, ..., 255 (logarithmic like the original firmware)
  if (config_->intensity <= 0)
    return 0;
  return uint8_t(0xFF >> (clockcore::kPatternBrightnessMax - config_->intensity));
}

void
Backlights::loop()
{
  if (config_ == nullptr)
    return;
  if (off_ || config_->pattern == Pattern::Off || config_->intensity == 0) {
    if (needsInit_) {
      clear();
      show();
      needsInit_ = false;
    }
    return;
  }
  switch (config_->pattern) {
    case Pattern::Constant:
      if (needsInit_) {
        fill(phaseToColor(uint16_t(config_->colorPhase)));
        setBrightness(brightness255());
        show();
      }
      break;
    case Pattern::Rainbow:
      rainbowPattern();
      break;
    case Pattern::Pulse:
      pulsePattern();
      break;
    case Pattern::Breath:
      breathPattern();
      break;
    default:
      break;
  }
  needsInit_ = false;
}

void
Backlights::pulsePattern()
{
  fill(phaseToColor(uint16_t(config_->colorPhase)));
  const float periodMs = 60000.0f / kPulseBpm;
  const float val = 1 + fabsf(sinf(2 * M_PI * millis() / periodMs)) * 254;
  setBrightness(uint8_t(val * brightness255() / 255));
  show();
}

void
Backlights::breathPattern()
{
  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  fill(phaseToColor(uint16_t(config_->colorPhase)));
  const float periodMs = 60000.0f / kBreathPerMin;
  const float val = (expf(sinf(2 * M_PI * millis() / periodMs)) - 0.36787944f) * 108.0f;
  uint8_t b = uint8_t(val * brightness255() / 255);
  setBrightness(b < 1 ? 1 : b);
  show();
}

void
Backlights::rainbowPattern()
{
  const uint16_t phasePerTube = (kMaxPhase / NUM_BACKLIGHT_LEDS) / 3;
  const uint16_t phase = uint16_t((millis() % kRainbowMs) * kMaxPhase / kRainbowMs);
  for (uint8_t screen = 0; screen < NUM_BACKLIGHT_LEDS; ++screen) {
    const uint16_t p = (phase + screen * phasePerTube) % kMaxPhase;
    setPixelColor(SCREEN_TO_DIGIT(screen), phaseToColor(p));
  }
  setBrightness(brightness255());
  show();
}

static uint8_t
phaseToIntensity(uint16_t phase)
{
  if (phase <= 255)
    return uint8_t(phase); // ramp up
  if (phase <= 511)
    return uint8_t(511 - phase); // ramp down
  return 0;                      // off
}

uint32_t
Backlights::phaseToColor(uint16_t phase)
{
  const uint8_t r = phaseToIntensity(phase % kMaxPhase);
  const uint8_t g = phaseToIntensity((phase + 256) % kMaxPhase);
  const uint8_t b = phaseToIntensity((phase + 512) % kMaxPhase);
  return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}
