// SPDX-License-Identifier: GPL-3.0-or-later
#include "Storage.hpp"

#include <LittleFS.h>

namespace
{
const char* kConfigPath = "/config.json";
const char* kConfigTmpPath = "/config.tmp";
bool gMounted = false;
} // namespace

namespace storage
{

bool begin()
{
    // Partition label "littlefs" (see partition_4MB.csv). Do not format on failure:
    // a failed mount must never wipe the user's images.
    gMounted = LittleFS.begin(false, "/littlefs", 5, "littlefs");
    if (!gMounted)
    {
        Serial.println("LittleFS: mount failed");
    }

    return gMounted;
}

bool mounted()
{
    return gMounted;
}

bool loadConfig(clockcore::Config& cfg)
{
    cfg = clockcore::Config();
    if (!gMounted)
    {
        return false;
    }

    File f = LittleFS.open(kConfigPath, "r");
    if (!f)
    {
        Serial.println("Config: /config.json not found, using defaults");
        return false;
    }

    std::string json;
    json.reserve(f.size());
    uint8_t buf[256];
    while (f.available())
    {
        size_t n = f.read(buf, sizeof(buf));
        json.append(reinterpret_cast<char*>(buf), n);
    }

    f.close();
    std::string err;
    if (!clockcore::parseConfig(json.data(), json.size(), cfg, &err))
    {
        Serial.printf("Config: invalid (%s), using defaults\n", err.c_str());
        return false;
    }

    return true;
}

bool saveConfig(const clockcore::Config& cfg)
{
    if (!gMounted)
    {
        return false;
    }

    const std::string json = clockcore::serializeConfig(cfg, true);
    File f = LittleFS.open(kConfigTmpPath, "w");
    if (!f)
    {
        return false;
    }

    const bool written = f.write(reinterpret_cast<const uint8_t*>(json.data()), json.size()) == json.size();
    f.close();
    if (!written)
    {
        LittleFS.remove(kConfigTmpPath);
        return false;
    }

    // littlefs rename replaces the target atomically.
    if (!LittleFS.rename(kConfigTmpPath, kConfigPath))
    {
        LittleFS.remove(kConfigPath);
        if (!LittleFS.rename(kConfigTmpPath, kConfigPath))
        {
            return false;
        }
    }

    return true;
}

size_t totalBytes()
{
    return gMounted ? LittleFS.totalBytes() : 0;
}

size_t usedBytes()
{
    return gMounted ? LittleFS.usedBytes() : 0;
}

std::vector<FileInfo> listFiles()
{
    std::vector<FileInfo> out;
    if (!gMounted)
    {
        return out;
    }

    File root = LittleFS.open("/");
    for (File f = root.openNextFile(); f; f = root.openNextFile())
    {
        if (!f.isDirectory())
        {
            out.push_back({ f.name(), f.size() });
        }

        f.close();
    }

    return out;
}

bool removeFile(const std::string& name)
{
    if (!gMounted || name.empty())
    {
        return false;
    }

    return LittleFS.remove(("/" + name).c_str());
}

} // namespace storage

void ImageLibrary::scan()
{
    index_.clear();
    for (const auto& f : storage::listFiles())
    {
        index_.add(f.name);
    }

    sets_ = index_.completeSets();
    select(current_);
    Serial.printf("Images: %u complete set(s)\n", unsigned(sets_.size()));
}

const std::string& ImageLibrary::select(const std::string& name)
{
    for (const auto& s : sets_)
    {
        if (s == name)
        {
            current_ = s;
            return current_;
        }
    }

    current_ = sets_.empty() ? std::string() : sets_.front();
    return current_;
}

int ImageLibrary::currentIndex() const
{
    for (size_t i = 0; i < sets_.size(); ++i)
    {
        if (sets_[i] == current_)
        {
            return int(i);
        }
    }

    return 0;
}

std::string ImageLibrary::path(int screen, int digit) const
{
    if (current_.empty())
    {
        return std::string();
    }

    std::string f = index_.resolve(current_, screen, digit);
    return f.empty() ? f : "/" + f;
}
