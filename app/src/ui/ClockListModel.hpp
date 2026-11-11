// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QTimer>

#include <memory>

namespace KDNSSD
{
class ServiceBrowser;
}

// Clocks on the local network (mDNS _tubeclock._tcp) merged with the clocks this app has
// set up before (which may have a static IP and no mDNS).
//
// Whether a clock is online is not taken from mDNS announcements (they are not reliable,
// e.g. a clock that lost power is still listed for a while): every clock is asked for
// GET /api/info directly, on refresh() and every few seconds. The answer also provides the
// clock's current IP address, model and firmware version.
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
        AddressRole, // IP address the clock answered at ("" while not connected)
        PortRole,
        ModelRole,
        FirmwareRole,
        OnlineRole,
        CheckingRole, // first check still running
        KnownRole
    };
    explicit ClockListModel(QObject* parent = nullptr);
    ~ClockListModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Restarts the mDNS search and checks every clock now.
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void countChanged();

private:
    struct Clock
    {
        QString name, host, address, model, firmware;
        int port = 80;
        bool online = false, known = false, checked = false, probing = false;
    };
    int upsert(const Clock& c);
    void loadKnown();
    void probeAll();
    void probe(const QString& name);
    int indexOf(const QString& name) const;

    QList<Clock> clocks_;
    std::unique_ptr<KDNSSD::ServiceBrowser> browser_;
    QTimer timer_;
};
