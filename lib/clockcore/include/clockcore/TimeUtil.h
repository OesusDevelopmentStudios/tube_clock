// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

namespace clockcore {

bool
isLeapYear(int year);
int
daysInMonth(int year, int month); // month 1..12

// Six values for the tubes, index 0 = leftmost tube.
struct Digits
{
  uint8_t v[6];
};

// HH MM SS. With twelveHour, hour 0 shows as 12 and 13..23 as 1..11.
Digits
timeDigits(int hour24, int minute, int second, bool twelveHour);
// DD MM YY
Digits
dateDigits(int day, int month, int year);

// ---- NTP (RFC 5905, client mode, SNTP subset) ----------------------------------

constexpr size_t kNtpPacketSize = 48;

// Fills a client request packet (version 4, mode 3).
void
buildNtpRequest(uint8_t (&packet)[kNtpPacketSize]);

// Extracts the server transmit time as Unix seconds. Rejects packets that are too
// short, not in server mode, unsynchronised (leap = 3 / stratum 0) or kiss-o'-death.
bool
parseNtpResponse(const uint8_t* packet, size_t len, uint32_t& unixSeconds);

// Decides when the clock may query the NTP server.
// - The first attempt is made as soon as the network is up.
// - After a successful sync the next one is due 24 h later (at most once per day).
// - After a failure: 1 min later until the first sync succeeded (right after the WiFi
//   connects, DNS is often not ready yet), 1 h later afterwards.
class NtpSchedule
{
public:
  static constexpr uint64_t kSyncIntervalMs = 24ULL * 60 * 60 * 1000;
  static constexpr uint64_t kRetryIntervalMs = 60ULL * 60 * 1000;
  static constexpr uint64_t kFirstSyncRetryMs = 60ULL * 1000;

  bool due(uint64_t nowMs) const { return !attempted_ || nowMs >= next_; }
  void onSuccess(uint64_t nowMs)
  {
    attempted_ = true;
    synced_ = true;
    lastSuccess_ = nowMs;
    next_ = nowMs + kSyncIntervalMs;
  }
  void onFailure(uint64_t nowMs)
  {
    attempted_ = true;
    next_ = nowMs + (synced_ ? kRetryIntervalMs : kFirstSyncRetryMs);
  }
  bool hasSynced() const { return synced_; }
  uint64_t lastSuccessMs() const { return lastSuccess_; }
  uint64_t nextAttemptMs() const { return next_; }

private:
  bool attempted_ = false;
  bool synced_ = false;
  uint64_t lastSuccess_ = 0;
  uint64_t next_ = 0;
};

} // namespace clockcore
