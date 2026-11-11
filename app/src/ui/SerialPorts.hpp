// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

// USB serial ports; ports with the USB bridges used on the clocks come first.
class SerialPorts : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles
    {
        PortRole = Qt::UserRole + 1,
        DescriptionRole,
        LikelyClockRole
    };
    explicit SerialPorts(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE int indexOf(const QString& port) const;

Q_SIGNALS:
    void countChanged();

private:
    struct Port
    {
        QString port, description;
        bool likelyClock;
    };
    QList<Port> ports_;
};
