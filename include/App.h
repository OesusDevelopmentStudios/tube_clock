// SPDX-License-Identifier: GPL-3.0-or-later
// Services of the main application used by the HTTP API (implemented in main.cpp).
#pragma once

#include <time.h>

#include <string>

#include "clockcore/Config.h"

class ImageLibrary;
class TimeKeeper;
class Network;

namespace app {

const clockcore::Config&
config();
// Validates, saves and applies a partial config document. Sets *rebootRequired when
// network settings changed (they are applied on the next start).
bool
patchConfig(const char* json, size_t len, std::string* error, bool* rebootRequired);
void
setUtcTime(time_t utc);
void
filesChanged(); // rescan image sets and redraw
void
scheduleReboot(uint32_t delayMs);
void
setDisplayPower(bool on);
bool
displayOn();

ImageLibrary&
images();
TimeKeeper&
timeKeeper();
Network&
network();
const std::string&
hostName();

} // namespace app
