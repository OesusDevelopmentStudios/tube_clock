// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeviceController.h"

#include <QDateTime>
#include <QJsonArray>

#include "AppSettings.h"
#include "HostResolver.h"
#include "TokenStore.h"
#include "clockcore/Validation.h"

DeviceController::DeviceController(QObject* parent)
  : QObject(parent)
{
  connect(&client_, &ClockClient::uploadProgress, this, [this](int done, int total) {
    uploadDone_ = done;
    uploadTotal_ = total;
    Q_EMIT uploadProgressChanged();
  });
}

void
DeviceController::setMessage(const QString& text, bool error)
{
  message_ = text;
  messageIsError_ = error;
  Q_EMIT messageChanged();
}

ClockClient::Callback
DeviceController::track(std::function<void(const ClockClient::Result&)> onOk, const QString& okMessage)
{
  ++pending_;
  Q_EMIT busyChanged();
  return [this, onOk, okMessage](const ClockClient::Result& r) {
    --pending_;
    Q_EMIT busyChanged();
    if (!r.ok) {
      if (r.status == 401)
        setMessage(QStringLiteral("The clock rejected the pairing token. Pair the clock again."), true);
      else
        setMessage(r.error, true);
      return;
    }
    if (!okMessage.isEmpty())
      setMessage(okMessage, false);
    if (onOk)
      onOk(r);
  };
}

void
DeviceController::open(const QString& name, const QString& host, int port)
{
  name_ = name;
  host_ = host;
  address_.clear();
  token_.clear();
  info_.clear();
  sets_.clear();
  client_.setToken(QString());
  Q_EMIT openedChanged();
  Q_EMIT infoChanged();
  Q_EMIT setsChanged();

  // Resolve the name first: "<name>.local" does not resolve inside the Flatpak sandbox
  // without Avahi (see HostResolver). Fall back to the address the clock last had.
  ++pending_;
  Q_EMIT busyChanged();
  const QString lastIp = AppSettings::instance()->knownClock(name).value(QStringLiteral("ip")).toString();
  HostResolver::resolve(host, [this, name, host, port, lastIp](const QString& resolved) {
    if (name != name_)
      return; // another clock was opened meanwhile
    --pending_;
    Q_EMIT busyChanged();
    address_ = !resolved.isEmpty() ? resolved : lastIp;
    Q_EMIT openedChanged();
    if (address_.isEmpty()) {
      setMessage(QStringLiteral("%1 was not found on the network.").arg(host.isEmpty() ? name : host), true);
      return;
    }
    client_.setEndpoint(address_, port);
    TokenStore::read(name, [this, name](const QString& token, const QString&) {
      if (name != name_)
        return;
      token_ = token;
      client_.setToken(token);
      Q_EMIT openedChanged();
      refresh();
    });
  });
}

void
DeviceController::pair(const QString& token)
{
  const QString t = token.trimmed().toLower();
  if (!clockcore::isValidToken(t.toStdString())) {
    setMessage(QStringLiteral("That is not a valid pairing token."), true);
    return;
  }
  token_ = t;
  client_.setToken(t);
  TokenStore::write(name_, t, nullptr);
  Q_EMIT openedChanged();
  refresh();
}

void
DeviceController::refresh()
{
  if (address_.isEmpty())
    return;
  client_.info(track([this](const ClockClient::Result& r) {
    info_ = r.body.toVariantMap();
    Q_EMIT infoChanged();
    // Remember clocks found on the network so a static IP keeps working later.
    AppSettings::instance()->rememberClock(name_,
                                           host_.isEmpty() ? address_ : host_,
                                           info_.value(QStringLiteral("model")).toString(),
                                           info_.value(QStringLiteral("ip")).toString());
  }));
  if (!paired())
    return;
  client_.getConfig(track([this](const ClockClient::Result& r) { config_.setConfig(ClockConfig::fromJson(r.body)); }));
  client_.listSets(track([this](const ClockClient::Result& r) {
    sets_ = r.body.value(QStringLiteral("sets")).toArray().toVariantList();
    Q_EMIT setsChanged();
  }));
}

void
DeviceController::syncTime()
{
  client_.setTime(QDateTime::currentSecsSinceEpoch(),
                  track([this](const auto&) { refresh(); }, QStringLiteral("The clock now has the computer's time.")));
}

void
DeviceController::applySettings()
{
  const QJsonObject all = config_.config().toJson(false);
  // Only the sections that apply immediately; network settings go through applyNetwork().
  QJsonObject patch{ { QStringLiteral("time"), all.value(QStringLiteral("time")) },
                     { QStringLiteral("display"), all.value(QStringLiteral("display")) },
                     { QStringLiteral("backlight"), all.value(QStringLiteral("backlight")) } };
  client_.patchConfig(patch, track([this](const auto&) { refresh(); }, QStringLiteral("Settings saved on the clock.")));
}

void
DeviceController::applyNetwork()
{
  const ClockConfig c = config_.config();
  QJsonObject wifi = c.toJson(true).value(QStringLiteral("wifi")).toObject();
  if (c.password.isEmpty())
    wifi.remove(QStringLiteral("password")); // keep the password stored on the clock
  client_.patchConfig({ { QStringLiteral("wifi"), wifi } },
                      track(
                        [this](const ClockClient::Result& r) {
                          if (r.body.value(QStringLiteral("reboot_required")).toBool())
                            reboot();
                        },
                        QStringLiteral("Network settings saved. The clock restarts to apply them.")));
}

void
DeviceController::disableRemoteAccess()
{
  client_.patchConfig({ { QStringLiteral("api"), QJsonObject{ { QStringLiteral("enabled"), false } } } },
                      track(nullptr,
                            QStringLiteral("Remote access is now off. Turn it on again in the clock's "
                                           "menu (Remote access) to use this app.")));
}

void
DeviceController::selectSet(const QString& name)
{
  client_.patchConfig({ { QStringLiteral("display"), QJsonObject{ { QStringLiteral("image_set"), name } } } },
                      track([this](const auto&) { refresh(); }));
}

void
DeviceController::deleteSet(const QString& name)
{
  client_.deleteSet(name, track([this](const auto&) { refresh(); }, QStringLiteral("Image set deleted.")));
}

void
DeviceController::uploadSet(int importRow)
{
  const QModelIndex idx = imports_.index(importRow);
  const QString setName = imports_.data(idx, ImageSetModel::NameRole).toString();
  const ImageSet* set = imports_.find(setName);
  if (set == nullptr)
    return;
  const qint64 freeBytes =
    info_.value(QStringLiteral("fs_total")).toLongLong() - info_.value(QStringLiteral("fs_used")).toLongLong();
  if (set->totalBytes() + 16 * 1024 > freeBytes) {
    setMessage(QStringLiteral("Not enough space on the clock. Delete a set first."), true);
    return;
  }
  client_.uploadFiles(set->files(),
                      track(
                        [this, importRow](const auto&) {
                          imports_.remove(importRow);
                          refresh();
                        },
                        QStringLiteral("Image set '%1' uploaded.").arg(setName)));
}

void
DeviceController::setDisplay(bool on)
{
  client_.setDisplay(on, track([this](const auto&) { refresh(); }));
}

void
DeviceController::reboot()
{
  client_.reboot(track(nullptr, QStringLiteral("The clock is restarting.")));
}

void
DeviceController::forget()
{
  TokenStore::remove(name_);
  AppSettings::instance()->forgetClock(name_);
  token_.clear();
  Q_EMIT openedChanged();
}
