// SPDX-License-Identifier: GPL-3.0-or-later
#include "clockcore/TzTable.h"

namespace clockcore {

int
tzIndexOf(const std::string& name)
{
  const TzEntry* t = tzTable();
  for (int i = 0; i < tzTableSize(); ++i)
    if (name == t[i].name)
      return i;
  return -1;
}

} // namespace clockcore
