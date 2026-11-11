// SPDX-License-Identifier: GPL-3.0-or-later
//
// Image sets on the clock's filesystem.
//
// File name: <set>_<screen>_<digit>.png
//   set    : [a-z0-9-]{1,20}, identifies the set in the clock menu
//   screen : 0..5, 0 = leftmost tube
//   digit  : 0..9
//
// If <set>_<screen>_<digit>.png is missing the clock falls back to <set>_0_<digit>.png,
// so a set where every tube looks the same needs only 10 files.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace clockcore
{

constexpr int kScreens = 6;
constexpr int kDigitsPerScreen = 10;
constexpr size_t kMaxSetNameLength = 20;

struct ImageName
{
    std::string set;
    int screen = 0;
    int digit = 0;
};

bool isValidSetName(const std::string& name);
// Accepts an optional leading '/'. The extension must be ".png" (lower case).
bool parseImageFileName(const std::string& fileName, ImageName& out);
std::string imageFileName(const std::string& set, int screen, int digit); // without leading '/'

class ImageSetIndex
{
public:
    void clear() { sets_.clear(); }
    // Registers a file found on the filesystem. Returns false if the name is not an image name.
    bool add(const std::string& fileName);
    void remove(const std::string& fileName);

    // Sets that can show every digit on every tube (with fallback), sorted by name.
    std::vector<std::string> completeSets() const;
    // All sets that have at least one image, sorted by name.
    std::vector<std::string> allSets() const;
    bool isComplete(const std::string& set) const;

    // File name (without '/') to draw for the tube/digit, or "" if the set cannot show it.
    std::string resolve(const std::string& set, int screen, int digit) const;

private:
    // bit (screen * 10 + digit) is set when the file exists
    std::map<std::string, uint64_t> sets_;
};

} // namespace clockcore
