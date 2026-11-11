// SPDX-License-Identifier: GPL-3.0-or-later
// WS2812 LEDs behind the tubes. Based on EleksTubeHAX by SmittyHalibut / aly-fly.
#pragma once

#include <Adafruit_NeoPixel.h>

#include "GLOBAL_DEFINES.hpp"
#include "clockcore/Config.hpp"

class Backlights : public Adafruit_NeoPixel
{
public:
    Backlights()
      : Adafruit_NeoPixel(NUM_BACKLIGHT_LEDS, BACKLIGHTS_PIN, NEO_GRB + NEO_KHZ800)
    {
    }

    void begin(clockcore::Config::Backlight* config);
    void loop();

    void powerOn() { setPowered(true); }
    void powerOff() { setPowered(false); }
    // Call after the config values changed.
    void refresh() { needsInit_ = true; }

    static uint32_t phaseToColor(uint16_t phase);

private:
    void setPowered(bool on)
    {
        off_ = !on;
        needsInit_ = true;
    }

    uint8_t brightness255() const;
    void rainbowPattern();
    void pulsePattern();
    void breathPattern();

    clockcore::Config::Backlight* config_ = nullptr;
    bool needsInit_ = true;
    bool off_ = false;

    static constexpr uint16_t kMaxPhase = 768;
    static constexpr uint8_t kPulseBpm = 60;
    static constexpr uint8_t kBreathPerMin = 20;
    static constexpr uint32_t kRainbowMs = 8000;
};
