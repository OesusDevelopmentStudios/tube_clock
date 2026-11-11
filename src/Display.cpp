// SPDX-License-Identifier: GPL-3.0-or-later
#include "Display.hpp"

#include <LittleFS.h>

#include "clockcore/Png565.hpp"

uint16_t Display::frame_[TFT_HEIGHT * TFT_WIDTH];

namespace
{
constexpr uint16_t kAccent = 0xFD20;    // orange
constexpr uint16_t kDim = 0xAD55;       // light grey
constexpr uint16_t kBarBg = 0x10A2;     // very dark grey
constexpr uint16_t kIconColor = 0x05FF; // light blue
constexpr int kLegendHeight = 34;
constexpr int kBadgeHeight = 20;
constexpr int kPad = 4;

// Greedy line break at ' ', '/' or '_' so the text fits the panel width.
template<typename Measure>
std::string takeLine(std::string& rest, Measure fits)
{
    std::string line = rest;
    while (line.size() > 1 && !fits(line))
    {
        size_t cut = line.find_last_of(" /_", line.size() - 2);
        line.resize(cut == std::string::npos || cut == 0 ? line.size() - 1 : cut + (line[cut] == '/' ? 1 : 0));
    }

    rest.erase(0, line.size());
    while (!rest.empty() && (rest[0] == ' ' || rest[0] == '_'))
    {
        rest.erase(0, 1);
    }
    while (!line.empty() && (line.back() == ' ' || line.back() == '_'))
    {
        line.pop_back();
    }

    return line;
}
} // namespace

void Display::begin()
{
    pinMode(TFT_ENABLE_PIN, OUTPUT);
    cs_.begin();
    powerOn();
}

void Display::powerOn()
{
    cs_.setAll(); // broadcast the init sequence to all panels
    init();
    setSwapBytes(true);
    fillScreen(TFT_BLACK);
    digitalWrite(TFT_ENABLE_PIN, ACTIVATEDISPLAYS);
    on_ = true;
    invalidate();
}

void Display::powerOff()
{
    cs_.setAll();
    fillScreen(TFT_BLACK);
    digitalWrite(TFT_ENABLE_PIN, DEACTIVATEDISPLAYS);
    on_ = false;
    invalidate();
}

void Display::setBrightnessLevel(int level)
{
    if (level < 1)
    {
        level = 1;
    }

    if (level > 10)
    {
        level = 10;
    }

    // 10 -> 255, 1 -> 30; the panels are unreadable below that.
    brightness_ = uint8_t(30 + (level - 1) * 25);
}

void Display::invalidate()
{
    for (auto& s : shown_)
    {
        s = ScreenState();
    }
}

void Display::select(int screen)
{
    cs_.setDigit(uint8_t(SCREEN_TO_DIGIT(screen)));
}

void Display::setOverlay(int screen, const Overlay& overlay)
{
    if (screen < 0 || screen >= NUM_DIGITS || overlay_[screen] == overlay)
    {
        return;
    }

    overlay_[screen] = overlay;
    // Bars may shrink, so the image underneath has to be sent again completely.
    shown_[screen] = ScreenState();
}

void Display::clearOverlays()
{
    for (int s = 0; s < NUM_DIGITS; ++s)
    {
        setOverlay(s, Overlay());
    }
}

Display::Bars Display::layout(const Overlay& o)
{
    Bars b;
    if (!o.title.empty())
    {
        int lines = 0;
        std::string rest = o.title;
        auto fits = [this](const std::string& l) { return textWidth(l.c_str(), 4) <= TFT_WIDTH - 2 * kPad; };
        while (!rest.empty() && lines < 3)
        {
            takeLine(rest, fits);
            ++lines;
        }

        b.top = kPad + lines * (fontHeight(4) + 2);
        if (!o.detail.empty())
        {
            b.top += fontHeight(2) + 2;
        }

        b.top += kPad + 2; // + accent line
    }

    if (o.icon != Icon::None)
    {
        b.bottom = kLegendHeight;
    }
    else if (!o.label.empty())
    {
        b.bottom = kBadgeHeight;
    }

    return b;
}

void Display::showDigit(int screen, int digit, const std::string& path)
{
    if (!on_ || screen < 0 || screen >= NUM_DIGITS)
    {
        return;
    }

    ScreenState& s = shown_[screen];
    const bool imageSame = s.digit == digit && s.path == path && s.brightness == brightness_;
    if (imageSame && s.overlayDrawn)
    {
        return;
    }

    select(screen);
    const Overlay& o = overlay_[screen];
    const Bars bars = layout(o);
    if (!imageSame)
    {
        if (!path.empty() && decodePng(path))
        {
            // Only the rows between the bars; the bars are drawn separately and stay untouched.
            const int h = TFT_HEIGHT - bars.top - bars.bottom;
            if (h > 0)
            {
                pushImage(0, bars.top, TFT_WIDTH, h, frame_ + size_t(bars.top) * TFT_WIDTH);
            }
        }
        else
        {
            drawFallbackDigit(digit, bars);
        }
    }

    if (!s.overlayDrawn)
    {
        drawOverlay(o, bars);
    }

    s.digit = digit;
    s.path = path;
    s.brightness = brightness_;
    s.overlayDrawn = true;
}

bool Display::decodePng(const std::string& path)
{
    fs::File f = LittleFS.open(path.c_str(), "r");
    if (!f)
    {
        return false;
    }

    clockcore::Png565Decoder dec(frame_, TFT_WIDTH, TFT_HEIGHT);
    bool ok = dec.begin(brightness_);
    uint8_t buf[1024];
    while (ok && f.available())
    {
        size_t n = f.read(buf, sizeof(buf));
        ok = dec.feed(buf, n);
    }

    f.close();
    ok = ok && dec.headerSeen();
    dec.end();
    if (!ok)
    {
        Serial.printf("PNG %s: %s\n", path.c_str(), dec.error() ? dec.error() : "failed");
    }

    return ok;
}

void Display::drawFallbackDigit(int digit, const Bars& bars)
{
    // Clipped to the area between the bars so it does not erase them.
    setViewport(0, bars.top, TFT_WIDTH, TFT_HEIGHT - bars.top - bars.bottom, false);
    fillRect(0, bars.top, TFT_WIDTH, TFT_HEIGHT - bars.top - bars.bottom, TFT_BLACK);
    setTextColor(alphaBlend(brightness_, kAccent, TFT_BLACK), TFT_BLACK);
    setTextDatum(MC_DATUM);
    setTextSize(2);
    const char s[2] = { char('0' + (digit % 10)), 0 };
    drawString(s, TFT_WIDTH / 2, TFT_HEIGHT / 2, 7);
    setTextSize(1);
    resetViewport();
}

int Display::drawWrapped(const std::string& text, int y, uint8_t font, uint16_t color, int maxLines)
{
    setTextColor(color, kBarBg);
    setTextDatum(TC_DATUM);
    std::string rest = text;
    auto fits = [this, font](const std::string& l) { return textWidth(l.c_str(), font) <= TFT_WIDTH - 2 * kPad; };
    for (int n = 0; n < maxLines && !rest.empty(); ++n)
    {
        const std::string line = takeLine(rest, fits);
        drawString(line.c_str(), TFT_WIDTH / 2, y, font);
        y += fontHeight(font) + 2;
    }

    return y;
}

void Display::drawOverlay(const Overlay& o, const Bars& bars)
{
    if (bars.top > 0)
    {
        fillRect(0, 0, TFT_WIDTH, bars.top, kBarBg);
        int y = drawWrapped(o.title, kPad, 4, o.titleColor, 3);
        if (!o.detail.empty())
        {
            drawWrapped(o.detail, y, 2, kDim, 1);
        }

        fillRect(0, bars.top - 2, TFT_WIDTH, 2, kAccent);
    }

    if (o.icon != Icon::None)
    {
        const int y0 = TFT_HEIGHT - kLegendHeight;
        fillRect(0, y0, TFT_WIDTH, kLegendHeight, kBarBg);
        fillRect(0, y0, TFT_WIDTH, 2, kAccent);
        const int cy = y0 + 2 + (kLegendHeight - 2) / 2;
        drawIcon(o.icon, 22, cy);
        setTextColor(TFT_WHITE, kBarBg);
        setTextDatum(ML_DATUM);
        drawString(o.label.c_str(), 44, cy + 1, 4);
    }
    else if (!o.label.empty())
    {
        const int y0 = TFT_HEIGHT - kBadgeHeight;
        fillRect(0, y0, TFT_WIDTH, kBadgeHeight, TFT_BLACK);
        setTextColor(TFT_RED, TFT_BLACK);
        setTextDatum(BC_DATUM);
        drawString(o.label.c_str(), TFT_WIDTH / 2, TFT_HEIGHT - 2, 2);
    }
}

void Display::drawIcon(Icon icon, int cx, int cy)
{
    switch (icon)
    {
        case Icon::Left:
            fillTriangle(cx - 10, cy, cx + 8, cy - 11, cx + 8, cy + 11, kIconColor);
            break;
        case Icon::Right:
            fillTriangle(cx + 10, cy, cx - 8, cy - 11, cx - 8, cy + 11, kIconColor);
            break;
        case Icon::Mode:
            fillCircle(cx, cy, 9, kIconColor);
            break;
        case Icon::Power:
            // Ring open at the top (TFT_eSPI angles: 0 = 6 o'clock, clockwise) and a bar.
            drawArc(cx, cy + 1, 11, 8, 0, 140, kIconColor, kBarBg, true);
            drawArc(cx, cy + 1, 11, 8, 220, 360, kIconColor, kBarBg, true);
            fillRoundRect(cx - 2, cy - 12, 4, 13, 2, kIconColor);
            break;
        case Icon::None:
            break;
    }
}

void Display::drawBootScreen(int screen, const char* title, const char* detail)
{
    select(screen);
    fillScreen(TFT_BLACK);
    Overlay o;
    o.title = title;
    o.detail = detail ? detail : "";
    o.titleColor = kAccent;
    drawOverlay(o, layout(o));
    shown_[screen] = ScreenState();
}
