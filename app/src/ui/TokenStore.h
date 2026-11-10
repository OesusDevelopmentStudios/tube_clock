// SPDX-License-Identifier: GPL-3.0-or-later
// Pairing tokens of the clocks, kept in the system keyring (KWallet / Secret Service).
#pragma once

#include <QString>

#include <functional>

namespace TokenStore {
void
write(const QString& clockName, const QString& token, std::function<void(bool ok, const QString& error)> done);
void
read(const QString& clockName, std::function<void(const QString& token, const QString& error)> done);
void
remove(const QString& clockName);
} // namespace TokenStore
