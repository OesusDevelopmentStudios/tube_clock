// SPDX-License-Identifier: GPL-3.0-or-later
#include "ClockListModel.h"

#include <KDNSSD/RemoteService>
#include <KDNSSD/ServiceBrowser>

#include "AppSettings.h"

ClockListModel::ClockListModel(QObject* parent)
  : QAbstractListModel(parent)
{
  connect(AppSettings::instance(), &AppSettings::knownClocksChanged, this, &ClockListModel::loadKnown);
  loadKnown();
  refresh();
}

ClockListModel::~ClockListModel() = default;

void
ClockListModel::loadKnown()
{
  for (const QVariant& v : AppSettings::instance()->knownClocks()) {
    const QVariantMap m = v.toMap();
    Clock c;
    c.name = m.value(QStringLiteral("name")).toString();
    c.host = m.value(QStringLiteral("host")).toString();
    c.model = m.value(QStringLiteral("model")).toString();
    c.known = true;
    upsert(c);
  }
}

void
ClockListModel::refresh()
{
  // Mark everything offline; the browser re-announces what is present.
  for (int i = 0; i < clocks_.size(); ++i)
    clocks_[i].online = false;
  if (!clocks_.isEmpty())
    Q_EMIT dataChanged(index(0), index(int(clocks_.size()) - 1), { OnlineRole });

  browser_ = std::make_unique<KDNSSD::ServiceBrowser>(QStringLiteral("_tubeclock._tcp"), true);
  connect(browser_.get(), &KDNSSD::ServiceBrowser::serviceAdded, this, [this](KDNSSD::RemoteService::Ptr s) {
    Clock c;
    const auto txt = s->textData();
    c.name = txt.contains("id") ? QString::fromUtf8(txt.value("id")) : s->serviceName();
    c.host = s->hostName();
    if (c.host.endsWith(QLatin1Char('.')))
      c.host.chop(1);
    c.port = s->port();
    c.model = QString::fromUtf8(txt.value("model"));
    c.firmware = QString::fromUtf8(txt.value("fw"));
    c.online = true;
    upsert(c);
  });
  connect(browser_.get(), &KDNSSD::ServiceBrowser::serviceRemoved, this, [this](KDNSSD::RemoteService::Ptr s) {
    for (int i = 0; i < clocks_.size(); ++i) {
      if (clocks_[i].name == s->serviceName() || clocks_[i].host.startsWith(s->serviceName())) {
        clocks_[i].online = false;
        Q_EMIT dataChanged(index(i), index(i), { OnlineRole });
      }
    }
  });
  browser_->startBrowse();
}

void
ClockListModel::upsert(const Clock& c)
{
  for (int i = 0; i < clocks_.size(); ++i) {
    Clock& e = clocks_[i];
    if (e.name == c.name) {
      if (!c.host.isEmpty() && (c.online || e.host.isEmpty()))
        e.host = c.host;
      if (!c.model.isEmpty())
        e.model = c.model;
      if (!c.firmware.isEmpty())
        e.firmware = c.firmware;
      e.port = c.port;
      e.online = e.online || c.online;
      e.known = e.known || c.known;
      Q_EMIT dataChanged(index(i), index(i));
      return;
    }
  }
  beginInsertRows({}, int(clocks_.size()), int(clocks_.size()));
  clocks_.append(c);
  endInsertRows();
  Q_EMIT countChanged();
}

int
ClockListModel::rowCount(const QModelIndex& parent) const
{
  return parent.isValid() ? 0 : int(clocks_.size());
}

QVariant
ClockListModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() >= clocks_.size())
    return {};
  const Clock& c = clocks_.at(index.row());
  switch (role) {
    case Qt::DisplayRole:
    case NameRole:
      return c.name;
    case HostRole:
      return c.host;
    case PortRole:
      return c.port;
    case ModelRole:
      return c.model;
    case FirmwareRole:
      return c.firmware;
    case OnlineRole:
      return c.online;
    case KnownRole:
      return c.known;
    default:
      return {};
  }
}

QHash<int, QByteArray>
ClockListModel::roleNames() const
{
  return { { NameRole, "name" },         { HostRole, "host" },     { PortRole, "port" },  { ModelRole, "model" },
           { FirmwareRole, "firmware" }, { OnlineRole, "online" }, { KnownRole, "known" } };
}
