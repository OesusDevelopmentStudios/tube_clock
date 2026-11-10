// SPDX-License-Identifier: GPL-3.0-or-later
//
// Streaming PNG decoder (pngle) that renders into an RGB565 frame buffer.
// The image is centered; uncovered pixels are black. Transparent pixels are
// blended over black. `brightness` (0..255) scales the colours, which is how
// LCD brightness is implemented on clocks without a backlight PWM pin.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct _pngle_t;

namespace clockcore {

inline uint16_t
rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

class Png565Decoder
{
public:
  Png565Decoder(uint16_t* buffer, int width, int height);
  ~Png565Decoder();
  Png565Decoder(const Png565Decoder&) = delete;
  Png565Decoder& operator=(const Png565Decoder&) = delete;

  // Starts a new image. Clears the frame buffer to black.
  bool begin(uint8_t brightness = 255);
  // Feeds the next chunk of the PNG stream. Returns false on a decode error.
  bool feed(const uint8_t* data, size_t len);
  // Releases the decoder memory (~45 KB on ESP32). Safe to call repeatedly.
  void end();

  bool headerSeen() const { return imageWidth_ > 0; }
  int imageWidth() const { return imageWidth_; }
  int imageHeight() const { return imageHeight_; }
  const char* error() const { return error_; }

  // Decodes a complete in-memory PNG (convenience for tests and small images).
  bool decode(const uint8_t* data, size_t len, uint8_t brightness = 255);

private:
  static void onInit(_pngle_t* p, uint32_t w, uint32_t h);
  static void onDraw(_pngle_t* p, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4]);

  uint16_t* buffer_;
  int width_, height_;
  _pngle_t* pngle_ = nullptr;
  uint8_t brightness_ = 255;
  int imageWidth_ = 0, imageHeight_ = 0;
  int offsetX_ = 0, offsetY_ = 0;
  const char* error_ = nullptr;
  std::vector<uint8_t> pending_; // bytes not yet consumed by pngle
};

} // namespace clockcore
