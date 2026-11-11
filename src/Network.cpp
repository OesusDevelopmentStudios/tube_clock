// SPDX-License-Identifier: GPL-3.0-or-later
#include "Network.hpp"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "GLOBAL_DEFINES.hpp"

namespace
{
// Reason of the last disconnect, reported by the WiFi driver (wifi_err_reason_t).
volatile uint8_t gLastReason = 0;

const char* reasonText(uint8_t reason)
{
    switch (reason)
    {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
            return "WiFi: bad password";
        case WIFI_REASON_NO_AP_FOUND:
            return "WiFi: not found";
        default:
            return "No WiFi";
    }
}
} // namespace

static IPAddress toIp(const std::string& s)
{
    IPAddress a;
    a.fromString(s.c_str());
    return a;
}

void Network::begin(const clockcore::Config::Wifi& wifi, const std::string& hostName)
{
    hostName_ = hostName;
    enabled_ = wifi.mode != clockcore::WifiMode::Off && !wifi.ssid.empty();
    if (!enabled_)
    {
        WiFi.mode(WIFI_OFF);
        Serial.println("WiFi: disabled");
        return;
    }

    WiFi.persistent(false); // credentials live in /config.json, not in the WiFi NVS
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(hostName_.c_str());
    if (wifi.mode == clockcore::WifiMode::Static)
    {
        const IPAddress dns = wifi.dns.empty() ? toIp(wifi.gateway) : toIp(wifi.dns);
        WiFi.config(toIp(wifi.ip), toIp(wifi.gateway), toIp(wifi.netmask), dns);
    }
    else
    {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }

    WiFi.onEvent(
        [](arduino_event_id_t, arduino_event_info_t info) {
            const uint8_t reason = info.wifi_sta_disconnected.reason;
            // Auto-reconnect retries several times a second; log only changes.
            if (reason != gLastReason)
            {
                Serial.printf("WiFi: disconnected, reason %u (%s)\n", unsigned(reason), reasonText(reason));
            }

            gLastReason = reason;
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.setAutoReconnect(true);
    WiFi.begin(wifi.ssid.c_str(), wifi.password.empty() ? nullptr : wifi.password.c_str());
    lastAttemptMs_ = downSinceMs_ = millis();
    Serial.printf("WiFi: connecting to %s (%s)\n", wifi.ssid.c_str(), clockcore::wifiModeName(wifi.mode));
}

void Network::loop()
{
    if (!enabled_)
    {
        return;
    }

    const uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED)
    {
        gLastReason = 0;
        if (downSinceMs_ != 0)
        {
            Serial.printf("WiFi: connected, IP %s\n", WiFi.localIP().toString().c_str());
            downSinceMs_ = 0;
            retryMs_ = 15000;
        }

        if (!mdnsStarted_)
        {
            startMdns();
        }

        return;
    }

    if (downSinceMs_ == 0)
    {
        downSinceMs_ = now == 0 ? 1 : now;
    }

    // Auto-reconnect normally handles drops; kick it with backoff if it stays down.
    if (now - lastAttemptMs_ >= retryMs_)
    {
        lastAttemptMs_ = now;
        retryMs_ = retryMs_ < 120000 ? retryMs_ * 2 : 120000;
        WiFi.reconnect();
    }
}

void Network::startMdns()
{
    if (!MDNS.begin(hostName_.c_str()))
    {
        Serial.println("mDNS: start failed");
        return;
    }

    MDNS.addService("tubeclock", "tcp", 80);
    MDNS.addServiceTxt("tubeclock", "tcp", "model", DEVICE_MODEL);
    MDNS.addServiceTxt("tubeclock", "tcp", "fw", FIRMWARE_VERSION);
    MDNS.addServiceTxt("tubeclock", "tcp", "id", hostName_.c_str());
    mdnsStarted_ = true;
    Serial.printf("mDNS: %s.local\n", hostName_.c_str());
}

const char* Network::offlineText() const
{
    if (!showOffline())
    {
        return "";
    }

    return reasonText(gLastReason);
}

bool Network::connected() const
{
    return enabled_ && WiFi.status() == WL_CONNECTED;
}

bool Network::showOffline() const
{
    return enabled_ && downSinceMs_ != 0 && millis() - downSinceMs_ > 60000;
}

std::string Network::ip() const
{
    return connected() ? WiFi.localIP().toString().c_str() : "";
}

int Network::rssi() const
{
    return connected() ? WiFi.RSSI() : 0;
}
