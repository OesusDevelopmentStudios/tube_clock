// SPDX-License-Identifier: GPL-3.0-or-later
#include "HostResolver.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QHostAddress>
#include <QHostInfo>

namespace HostResolver {

namespace {

void
resolveWithQt(const QString& host, std::function<void(const QString&)> done)
{
  QHostInfo::lookupHost(host, [done](const QHostInfo& info) {
    for (const QHostAddress& a : info.addresses())
      if (a.protocol() == QAbstractSocket::IPv4Protocol) {
        done(a.toString());
        return;
      }
    done(QString());
  });
}

} // namespace

void
resolve(const QString& host, std::function<void(const QString&)> done)
{
  if (QHostAddress(host).protocol() == QAbstractSocket::IPv4Protocol) {
    done(host);
    return;
  }
  if (!host.endsWith(QLatin1String(".local"))) {
    resolveWithQt(host, done);
    return;
  }
  // org.freedesktop.Avahi.Server.ResolveHostName(interface, protocol, name, aprotocol, flags)
  // -1 = any interface / protocol, aprotocol 0 = IPv4 address.
  QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Avahi"),
                                                    QStringLiteral("/"),
                                                    QStringLiteral("org.freedesktop.Avahi.Server"),
                                                    QStringLiteral("ResolveHostName"));
  msg << qint32(-1) << qint32(-1) << host << qint32(0) << quint32(0);
  auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg, 5000));
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, [host, done](QDBusPendingCallWatcher* w) {
    w->deleteLater();
    const QDBusMessage reply = w->reply();
    if (reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() >= 5) {
      const QString address = reply.arguments().at(4).toString();
      if (QHostAddress(address).protocol() == QAbstractSocket::IPv4Protocol) {
        done(address);
        return;
      }
    }
    // No Avahi (or not found): the C library may still manage outside the sandbox.
    resolveWithQt(host, done);
  });
}

} // namespace HostResolver
