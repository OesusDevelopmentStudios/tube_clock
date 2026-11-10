// SPDX-License-Identifier: GPL-3.0-or-later
//
// The six IPS displays. All public methods use logical screens (0 = leftmost tube).
//
// Every screen shows a digit image. Menus and status messages are drawn as overlay
// bars on top of it (a top bar for text, a bottom bar for the button legend or a
// status badge), so the digits stay visible while settings change. When only the
// digit changes, just the image rows outside the bars are sent to the panel, which
// keeps the bars flicker free.
#pragma once

#include <TFT_eSPI.h>

#include <string>

#include "ChipSelect.h"
#include "GLOBAL_DEFINES.h"

class Display : public TFT_eSPI
{
public:
  enum class Icon : uint8_t
  {
    None,
    Left,
    Mode,
    Right,
    Power
  };

  struct Overlay
  {
    // Top bar: title (font 4, wrapped) and optional detail line (font 2).
    std::string title;
    std::string detail;
    uint16_t titleColor = TFT_WHITE;
    // Bottom bar: legend (icon + label) or, if no icon, a red status badge.
    Icon icon = Icon::None;
    std::string label;

    bool operator==(const Overlay& o) const
    {
      return title == o.title && detail == o.detail && titleColor == o.titleColor && icon == o.icon && label == o.label;
    }
    bool operator!=(const Overlay& o) const { return !(*this == o); }
  };

  void begin();

  void powerOn();  // re-initialises the panels (they lose state when unpowered)
  void powerOff(); // cuts panel power via the MOSFET; the clock keeps running
  bool isOn() const { return on_; }

  // 1..10
  void setBrightnessLevel(int level);

  // Draws the image for `digit` on `screen` (if anything changed) plus its overlay.
  // `path` is the LittleFS path from ImageLibrary; empty -> built-in 7-segment digit.
  void showDigit(int screen, int digit, const std::string& path);
  void setOverlay(int screen, const Overlay& overlay);
  void clearOverlays();
  // Forget what is shown so the next showDigit() redraws everything.
  void invalidate();

  // Text-only screen used while booting.
  void drawBootScreen(int screen, const char* title, const char* detail);

private:
  struct Bars
  {
    int top = 0;    // height of the top bar in px
    int bottom = 0; // height of the bottom bar in px
  };

  void select(int screen);
  bool decodePng(const std::string& path);
  void drawFallbackDigit(int digit, const Bars& bars);
  Bars layout(const Overlay& o);
  void drawOverlay(const Overlay& o, const Bars& bars);
  void drawIcon(Icon icon, int cx, int cy);
  int drawWrapped(const std::string& text, int y, uint8_t font, uint16_t color, int maxLines);

  ChipSelect cs_;
  bool on_ = false;
  uint8_t brightness_ = 255;

  struct ScreenState
  {
    int digit = -1;
    std::string path;
    uint8_t brightness = 0;
    bool overlayDrawn = false;
  } shown_[NUM_DIGITS];
  Overlay overlay_[NUM_DIGITS];

  static uint16_t frame_[TFT_HEIGHT * TFT_WIDTH];
};
