// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

#include <memory>

namespace KDNSSD {
class ServiceBrowser;
}

// Clocks on the local network (mDNS _tubeclock._tcp) merged with the clocks
// this app has set up before (which may have a static IP and no mDNS).
class ClockListModel : public QAbstractListModel
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
  enum Roles
  {
    NameRole = Qt::UserRole + 1,
    HostRole,
    PortRole,
    ModelRole,
    FirmwareRole,
    OnlineRole,
    KnownRole
  };
  explicit ClockListModel(QObject* parent = nullptr);
  ~ClockListModel() override;

  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void refresh();

Q_SIGNALS:
  void countChanged();

private:
  struct Clock
  {
    QString name, host, model, firmware;
    int port = 80;
    bool online = false, known = false;
  };
  void upsert(const Clock& c);
  void loadKnown();
  QList<Clock> clocks_;
  std::unique_ptr<KDNSSD::ServiceBrowser> browser_;
};
