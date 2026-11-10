// SPDX-License-Identifier: GPL-3.0-or-later
//
// Resolves clock host names to IP addresses.
//
// "<name>.local" (mDNS) names cannot be resolved by the C library inside the Flatpak
// sandbox (the runtime has no nss-mdns), so they are resolved through the Avahi daemon
// on the system D-Bus, which the sandbox may talk to. Other names and plain addresses go
// through QHostInfo.
#pragma once

#include <QString>

#include <functional>

namespace HostResolver {

// Calls `done` with an IPv4 address, or an empty string if the name could not be resolved.
void
resolve(const QString& host, std::function<void(const QString& address)> done);

} // namespace HostResolver
