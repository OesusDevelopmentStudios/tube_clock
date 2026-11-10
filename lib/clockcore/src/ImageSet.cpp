// SPDX-License-Identifier: GPL-3.0-or-later
#include "clockcore/ImageSet.h"

namespace clockcore {

namespace {
uint64_t
bitFor(int screen, int digit)
{
  return 1ULL << (screen * kDigitsPerScreen + digit);
}
} // namespace

bool
isValidSetName(const std::string& name)
{
  if (name.empty() || name.size() > kMaxSetNameLength)
    return false;
  for (char c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
      return false;
  }
  return true;
}

bool
parseImageFileName(const std::string& fileName, ImageName& out)
{
  std::string n = fileName;
  if (!n.empty() && n[0] == '/')
    n.erase(0, 1);
  const std::string ext = ".png";
  if (n.size() <= ext.size() || n.compare(n.size() - ext.size(), ext.size(), ext) != 0)
    return false;
  n.resize(n.size() - ext.size());
  // <set>_<screen>_<digit>: screen and digit are single characters
  if (n.size() < 5 || n[n.size() - 2] != '_' || n[n.size() - 4] != '_')
    return false;
  const char s = n[n.size() - 3];
  const char d = n[n.size() - 1];
  if (s < '0' || s >= '0' + kScreens || d < '0' || d > '9')
    return false;
  std::string set = n.substr(0, n.size() - 4);
  if (!isValidSetName(set))
    return false;
  out.set = set;
  out.screen = s - '0';
  out.digit = d - '0';
  return true;
}

std::string
imageFileName(const std::string& set, int screen, int digit)
{
  std::string n = set;
  n += '_';
  n += static_cast<char>('0' + screen);
  n += '_';
  n += static_cast<char>('0' + digit);
  n += ".png";
  return n;
}

bool
ImageSetIndex::add(const std::string& fileName)
{
  ImageName in;
  if (!parseImageFileName(fileName, in))
    return false;
  sets_[in.set] |= bitFor(in.screen, in.digit);
  return true;
}

void
ImageSetIndex::remove(const std::string& fileName)
{
  ImageName in;
  if (!parseImageFileName(fileName, in))
    return;
  auto it = sets_.find(in.set);
  if (it == sets_.end())
    return;
  it->second &= ~bitFor(in.screen, in.digit);
  if (it->second == 0)
    sets_.erase(it);
}

std::vector<std::string>
ImageSetIndex::completeSets() const
{
  std::vector<std::string> out;
  for (const auto& kv : sets_)
    if (isComplete(kv.first))
      out.push_back(kv.first);
  return out;
}

std::vector<std::string>
ImageSetIndex::allSets() const
{
  std::vector<std::string> out;
  for (const auto& kv : sets_)
    out.push_back(kv.first);
  return out;
}

bool
ImageSetIndex::isComplete(const std::string& set) const
{
  for (int screen = 0; screen < kScreens; ++screen)
    for (int digit = 0; digit < kDigitsPerScreen; ++digit)
      if (resolve(set, screen, digit).empty())
        return false;
  return true;
}

std::string
ImageSetIndex::resolve(const std::string& set, int screen, int digit) const
{
  if (screen < 0 || screen >= kScreens || digit < 0 || digit >= kDigitsPerScreen)
    return std::string();
  auto it = sets_.find(set);
  if (it == sets_.end())
    return std::string();
  if (it->second & bitFor(screen, digit))
    return imageFileName(set, screen, digit);
  if (it->second & bitFor(0, digit))
    return imageFileName(set, 0, digit);
  return std::string();
}

} // namespace clockcore
