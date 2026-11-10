// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QUrl>

#include "ImageSet.h"

// Image sets prepared in the app (bundled defaults and user imports).
// Previews: image://tubeclock/<modelId>/<set>/<screen>/<digit>
class ImageSetModel : public QAbstractListModel
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int count READ rowCount NOTIFY changed)
  Q_PROPERTY(qint64 includedBytes READ includedBytes NOTIFY changed)
  Q_PROPERTY(QStringList includedNames READ includedNames NOTIFY changed)
  Q_PROPERTY(int modelId READ modelId CONSTANT)

public:
  enum Roles
  {
    NameRole = Qt::UserRole + 1,
    IncludedRole,
    CompleteRole,
    BytesRole,
    BuiltinRole,
    PerTubeRole
  };
  explicit ImageSetModel(QObject* parent = nullptr);
  ~ImageSetModel() override;

  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  QHash<int, QByteArray> roleNames() const override;

  // Adds the sets shipped with the app (share/tubeclock/imagesets).
  Q_INVOKABLE void loadBundled(bool included = true);
  // Imports pictures; fit: 0 contain, 1 cover, 2 stretch. Returns "" or an error text.
  Q_INVOKABLE QString importFiles(const QList<QUrl>& files, const QString& name, int fit, const QColor& background);
  Q_INVOKABLE void remove(int row);
  Q_INVOKABLE void setIncluded(int row, bool included);
  Q_INVOKABLE QString sanitizeName(const QString& name) const { return ImageTools::sanitizeSetName(name); }

  qint64 includedBytes() const;
  QStringList includedNames() const;
  QList<ImageSet> includedSets() const;
  const ImageSet* find(const QString& name) const;
  int modelId() const { return id_; }

  static QString bundledDir();
  static ImageSetModel* byId(int id);

Q_SIGNALS:
  void changed();

private:
  struct Entry
  {
    ImageSet set;
    bool included = true;
    bool builtin = false;
  };
  QList<Entry> entries_;
  int id_;
};

class SetPreviewProvider : public QQuickImageProvider
{
public:
  SetPreviewProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
  {
  }
  QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
