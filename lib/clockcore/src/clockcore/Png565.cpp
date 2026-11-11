// SPDX-License-Identifier: GPL-3.0-or-later
#include "clockcore/Png565.hpp"

#include <cstring>

#include <pngle.h>

namespace clockcore
{

Png565Decoder::Png565Decoder(uint16_t* buffer, int width, int height)
  : buffer_(buffer)
  , width_(width)
  , height_(height)
{
}

Png565Decoder::~Png565Decoder()
{
    end();
}

bool Png565Decoder::begin(uint8_t brightness)
{
    end();
    brightness_ = brightness;
    imageWidth_ = imageHeight_ = 0;
    offsetX_ = offsetY_ = 0;
    error_ = nullptr;
    std::memset(buffer_, 0, sizeof(uint16_t) * size_t(width_) * size_t(height_));
    pngle_ = pngle_new();
    if (pngle_ == nullptr)
    {
        error_ = "out of memory";
        return false;
    }

    pngle_set_user_data(pngle_, this);
    pngle_set_init_callback(pngle_, &Png565Decoder::onInit);
    pngle_set_draw_callback(pngle_, &Png565Decoder::onDraw);
    return true;
}

bool Png565Decoder::feed(const uint8_t* data, size_t len)
{
    if (pngle_ == nullptr)
    {
        if (error_ == nullptr)
        {
            error_ = "decoder not started";
        }

        return false;
    }

    // pngle may consume less than offered (it needs whole chunk headers etc.).
    // The unconsumed tail is kept and fed again together with the next data.
    pending_.insert(pending_.end(), data, data + len);
    size_t pos = 0;
    while (pos < pending_.size())
    {
        int n = pngle_feed(pngle_, pending_.data() + pos, pending_.size() - pos);
        if (n < 0)
        {
            error_ = pngle_error(pngle_);
            pending_.clear();
            return false;
        }

        if (n == 0)
        {
            break;
        }

        pos += size_t(n);
    }

    pending_.erase(pending_.begin(), pending_.begin() + long(pos));
    return true;
}

void Png565Decoder::end()
{
    pending_.clear();
    pending_.shrink_to_fit();
    if (pngle_ != nullptr)
    {
        pngle_destroy(pngle_);
        pngle_ = nullptr;
    }
}

bool Png565Decoder::decode(const uint8_t* data, size_t len, uint8_t brightness)
{
    if (!begin(brightness))
    {
        return false;
    }

    bool ok = feed(data, len);
    if (ok && !headerSeen())
    {
        error_ = "no image header";
        ok = false;
    }

    end();
    return ok;
}

void Png565Decoder::onInit(_pngle_t* p, uint32_t w, uint32_t h)
{
    auto* self = static_cast<Png565Decoder*>(pngle_get_user_data(p));
    self->imageWidth_ = int(w);
    self->imageHeight_ = int(h);
    // Center the image. Larger images are cropped symmetrically.
    self->offsetX_ = (self->width_ - int(w)) / 2;
    self->offsetY_ = (self->height_ - int(h)) / 2;
}

void Png565Decoder::onDraw(_pngle_t* p, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4])
{
    auto* self = static_cast<Png565Decoder*>(pngle_get_user_data(p));
    // Alpha blend over black, then apply brightness: c * a/255 * b/255.
    const uint32_t scale = uint32_t(rgba[3]) * self->brightness_; // 0..65025
    const uint8_t r = uint8_t((rgba[0] * scale) / 65025);
    const uint8_t g = uint8_t((rgba[1] * scale) / 65025);
    const uint8_t b = uint8_t((rgba[2] * scale) / 65025);
    const uint16_t c = rgb565(r, g, b);
    // w/h > 1 only for interlaced previews; fill the whole block.
    for (uint32_t dy = 0; dy < h; ++dy)
    {
        const int ty = self->offsetY_ + int(y + dy);
        if (ty < 0 || ty >= self->height_)
        {
            continue;
        }

        for (uint32_t dx = 0; dx < w; ++dx)
        {
            const int tx = self->offsetX_ + int(x + dx);
            if (tx < 0 || tx >= self->width_)
            {
                continue;
            }

            self->buffer_[size_t(ty) * size_t(self->width_) + size_t(tx)] = c;
        }
    }
}

} // namespace clockcore
