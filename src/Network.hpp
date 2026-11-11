// SPDX-License-Identifier: GPL-3.0-or-later
// WiFi station (DHCP or static IP) and mDNS announcement.
#pragma once

#include <Arduino.h>

#include <string>

#include "clockcore/Config.hpp"

class Network
{
public:
    void begin(const clockcore::Config::Wifi& wifi, const std::string& hostName);
    void loop();

    bool enabled() const { return enabled_; }
    bool connected() const;
    // True if WiFi is enabled but has been down for a while (for the status badge).
    bool showOffline() const;
    // Status badge text while offline, naming the likely cause ("WiFi: bad password"); "" when fine.
    const char* offlineText() const;
    std::string ip() const;
    int rssi() const;

private:
    void startMdns();

    bool enabled_ = false;
    bool mdnsStarted_ = false;
    std::string hostName_;
    uint32_t lastAttemptMs_ = 0;
    uint32_t downSinceMs_ = 0;
    uint32_t retryMs_ = 15000;
};
