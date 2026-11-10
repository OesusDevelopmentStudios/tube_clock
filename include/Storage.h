// SPDX-License-Identifier: GPL-3.0-or-later
//
// LittleFS backed storage: /config.json and the image sets (<set>_<screen>_<digit>.png).
#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

#include "clockcore/Config.h"
#include "clockcore/ImageSet.h"

namespace storage {

bool
begin(); // mounts LittleFS (never formats)
bool
mounted();

// Loads /config.json. Missing or invalid files yield the defaults (returns false).
bool
loadConfig(clockcore::Config& cfg);
// Writes /config.json atomically (temp file + rename).
bool
saveConfig(const clockcore::Config& cfg);

size_t
totalBytes();
size_t
usedBytes();

struct FileInfo
{
  std::string name;
  size_t size;
};
std::vector<FileInfo>
listFiles();
bool
removeFile(const std::string& name);

} // namespace storage

// Index of the image sets on the filesystem and the selected set.
class ImageLibrary
{
public:
  void scan();
  const std::vector<std::string>& sets() const { return sets_; } // complete sets, sorted
  const clockcore::ImageSetIndex& index() const { return index_; }

  // Selects a set by name; falls back to the first complete set. Returns the active name.
  const std::string& select(const std::string& name);
  const std::string& current() const { return current_; }
  int currentIndex() const;

  // "/<file>" for the tube and digit, or "" when no set can show it.
  std::string path(int screen, int digit) const;

private:
  clockcore::ImageSetIndex index_;
  std::vector<std::string> sets_;
  std::string current_;
};
