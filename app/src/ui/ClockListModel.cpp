// SPDX-License-Identifier: GPL-3.0-or-later
#include "ClockListModel.hpp"

#include <KDNSSD/RemoteService>
#include <KDNSSD/ServiceBrowser>

#include "AppSettings.hpp"
#include "ClockClient.hpp"
#include "HostResolver.hpp"

namespace
{
constexpr int kProbeIntervalMs = 15000;
constexpr int kProbeTimeoutMs = 3000;
} // namespace

ClockListModel::ClockListModel(QObject* parent)
  : QAbstractListModel(parent)
{
    connect(AppSettings::instance(), &AppSettings::knownClocksChanged, this, &ClockListModel::loadKnown);
    loadKnown();
    timer_.setInterval(kProbeIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &ClockListModel::probeAll);
    timer_.start();
    refresh();
}

ClockListModel::~ClockListModel() = default;

void ClockListModel::loadKnown()
{
    for (const QVariant& v : AppSettings::instance()->knownClocks())
    {
        const QVariantMap m = v.toMap();
        Clock c;
        c.name = m.value(QStringLiteral("name")).toString();
        c.host = m.value(QStringLiteral("host")).toString();
        c.model = m.value(QStringLiteral("model")).toString();
        c.known = true;
        upsert(c);
    }
}

void ClockListModel::refresh()
{
    browser_ = std::make_unique<KDNSSD::ServiceBrowser>(QStringLiteral("_tubeclock._tcp"), true);
    connect(browser_.get(), &KDNSSD::ServiceBrowser::serviceAdded, this, [this](KDNSSD::RemoteService::Ptr s) {
        Clock c;
        const auto txt = s->textData();
        c.name = txt.contains("id") ? QString::fromUtf8(txt.value("id")) : s->serviceName();
        c.host = s->hostName();
        if (c.host.endsWith(QLatin1Char('.')))
        {
            c.host.chop(1);
        }

        c.port = s->port();
        c.model = QString::fromUtf8(txt.value("model"));
        c.firmware = QString::fromUtf8(txt.value("fw"));
        upsert(c);
        probe(c.name); // an announcement alone does not mean the clock is reachable
    });
    browser_->startBrowse();
    probeAll();
}

int ClockListModel::indexOf(const QString& name) const
{
    for (int i = 0; i < clocks_.size(); ++i)
    {
        if (clocks_.at(i).name == name)
        {
            return i;
        }
    }

    return -1;
}

int ClockListModel::upsert(const Clock& c)
{
    const int i = indexOf(c.name);
    if (i >= 0)
    {
        Clock& e = clocks_[i];
        if (!c.host.isEmpty())
        {
            e.host = c.host;
        }

        if (!c.model.isEmpty())
        {
            e.model = c.model;
        }

        if (!c.firmware.isEmpty())
        {
            e.firmware = c.firmware;
        }

        e.port = c.port;
        e.known = e.known || c.known;
        Q_EMIT dataChanged(index(i), index(i));
        return i;
    }

    beginInsertRows({}, int(clocks_.size()), int(clocks_.size()));
    clocks_.append(c);
    endInsertRows();
    Q_EMIT countChanged();
    probe(c.name);
    return int(clocks_.size()) - 1;
}

void ClockListModel::probeAll()
{
    for (const Clock& c : std::as_const(clocks_))
    {
        probe(c.name);
    }
}

void ClockListModel::probe(const QString& name)
{
    const int i = indexOf(name);
    if (i < 0 || clocks_.at(i).probing)
    {
        return;
    }

    clocks_[i].probing = true;
    const QString host = clocks_.at(i).host;
    const int port = clocks_.at(i).port;
    const QString lastIp = AppSettings::instance()->knownClock(name).value(QStringLiteral("ip")).toString();

    // Rows may move while the asynchronous steps run, so look the clock up by name again.
    auto finish = [this, name](bool online, const QJsonObject& info) {
        const int row = indexOf(name);
        if (row < 0)
        {
            return;
        }

        Clock& c = clocks_[row];
        c.probing = false;
        c.checked = true;
        c.online = online;
        c.address = online ? info.value(QStringLiteral("ip")).toString() : QString();
        if (online)
        {
            if (info.contains(QStringLiteral("model")))
            {
                c.model = info.value(QStringLiteral("model")).toString();
            }

            if (info.contains(QStringLiteral("firmware")))
            {
                c.firmware = info.value(QStringLiteral("firmware")).toString();
            }

            if (c.known && !c.address.isEmpty()
                && AppSettings::instance()->knownClock(name).value(QStringLiteral("ip")).toString() != c.address)
            {
                AppSettings::instance()->rememberClock(name, c.host, c.model, c.address);
            }
        }

        Q_EMIT dataChanged(index(row), index(row));
    };

    HostResolver::resolve(host, [this, finish, port, lastIp](const QString& resolved) {
        const QString address = resolved.isEmpty() ? lastIp : resolved;
        if (address.isEmpty())
        {
            finish(false, {});
            return;
        }

        auto* client = new ClockClient(this);
        client->setEndpoint(address, port);
        client->setTimeoutMs(kProbeTimeoutMs);
        client->info([client, finish](const ClockClient::Result& r) {
            client->deleteLater();
            // Only a Tube Clock answer counts (something else may use the old address now).
            finish(r.ok && r.body.contains(QStringLiteral("api_version")), r.body);
        });
    });
}

int ClockListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(clocks_.size());
}

QVariant ClockListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= clocks_.size())
    {
        return {};
    }

    const Clock& c = clocks_.at(index.row());
    switch (role)
    {
        case Qt::DisplayRole:
        case NameRole:
            return c.name;
        case HostRole:
            return c.host;
        case AddressRole:
            return c.address;
        case PortRole:
            return c.port;
        case ModelRole:
            return c.model;
        case FirmwareRole:
            return c.firmware;
        case OnlineRole:
            return c.online;
        case CheckingRole:
            return !c.checked;
        case KnownRole:
            return c.known;
        default:
            return {};
    }
}

QHash<int, QByteArray> ClockListModel::roleNames() const
{
    return { { NameRole, "name" },
        { HostRole, "host" },
        { AddressRole, "address" },
        { PortRole, "port" },
        { ModelRole, "model" },
        { FirmwareRole, "firmware" },
        { OnlineRole, "online" },
        { CheckingRole, "checking" },
        { KnownRole, "known" } };
}
