// SPDX-License-Identifier: GPL-3.0-or-later
#include "SerialPorts.h"

#include "SerialDiscovery.h"

SerialPorts::SerialPorts(QObject* parent)
  : QAbstractListModel(parent)
{
  refresh();
}

int
SerialPorts::rowCount(const QModelIndex& parent) const
{
  return parent.isValid() ? 0 : int(ports_.size());
}

QVariant
SerialPorts::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() >= ports_.size())
    return {};
  const Port& p = ports_.at(index.row());
  switch (role) {
    case Qt::DisplayRole:
      return p.description.isEmpty() ? p.port : QStringLiteral("%1 (%2)").arg(p.port, p.description);
    case PortRole:
      return p.port;
    case DescriptionRole:
      return p.description;
    case LikelyClockRole:
      return p.likelyClock;
    default:
      return {};
  }
}

QHash<int, QByteArray>
SerialPorts::roleNames() const
{
  return { { Qt::DisplayRole, "display" },
           { PortRole, "port" },
           { DescriptionRole, "description" },
           { LikelyClockRole, "likelyClock" } };
}

void
SerialPorts::refresh()
{
  beginResetModel();
  ports_.clear();
  // Works without udev (Flatpak): see SerialDiscovery.
  for (const SerialDiscovery::Port& p : SerialDiscovery::scan())
    ports_.append({ p.path, p.description, p.likelyClock });
  endResetModel();
  Q_EMIT countChanged();
}

int
SerialPorts::indexOf(const QString& port) const
{
  for (int i = 0; i < ports_.size(); ++i)
    if (ports_.at(i).port == port)
      return i;
  return -1;
}
