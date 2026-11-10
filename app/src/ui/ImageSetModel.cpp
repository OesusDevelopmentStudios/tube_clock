// SPDX-License-Identifier: GPL-3.0-or-later
#include "ImageSetModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#include "ClockFilesystem.h"

namespace {
QHash<int, ImageSetModel*>&
registry()
{
  static QHash<int, ImageSetModel*> r;
  return r;
}
int gNextId = 1;
} // namespace

ImageSetModel::ImageSetModel(QObject* parent)
  : QAbstractListModel(parent)
  , id_(gNextId++)
{
  registry().insert(id_, this);
}

ImageSetModel::~ImageSetModel()
{
  registry().remove(id_);
}

ImageSetModel*
ImageSetModel::byId(int id)
{
  return registry().value(id);
}

int
ImageSetModel::rowCount(const QModelIndex& parent) const
{
  return parent.isValid() ? 0 : int(entries_.size());
}

QVariant
ImageSetModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() >= entries_.size())
    return {};
  const Entry& e = entries_.at(index.row());
  switch (role) {
    case Qt::DisplayRole:
    case NameRole:
      return e.set.name();
    case IncludedRole:
      return e.included;
    case CompleteRole:
      return e.set.isComplete();
    case BytesRole:
      return ClockFilesystem::estimateBytes(e.set);
    case BuiltinRole:
      return e.builtin;
    case PerTubeRole:
      return e.set.files().size() > ImageTools::kDigits;
    default:
      return {};
  }
}

bool
ImageSetModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (!index.isValid() || role != IncludedRole)
    return false;
  setIncluded(index.row(), value.toBool());
  return true;
}

QHash<int, QByteArray>
ImageSetModel::roleNames() const
{
  return { { NameRole, "name" },   { IncludedRole, "included" }, { CompleteRole, "complete" },
           { BytesRole, "bytes" }, { BuiltinRole, "builtin" },   { PerTubeRole, "perTube" } };
}

QString
ImageSetModel::bundledDir()
{
  const QByteArray env = qgetenv("TUBECLOCK_IMAGESETS_DIR");
  if (!env.isEmpty())
    return QString::fromLocal8Bit(env);
  const QString found = QStandardPaths::locate(
    QStandardPaths::GenericDataLocation, QStringLiteral("tubeclock/imagesets"), QStandardPaths::LocateDirectory);
  if (!found.isEmpty())
    return found;
  return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../share/tubeclock/imagesets"));
}

void
ImageSetModel::loadBundled(bool included)
{
  const QList<ImageSet> sets = ImageSet::fromClockFolder(bundledDir());
  beginResetModel();
  for (const ImageSet& s : sets) {
    if (find(s.name()) == nullptr)
      entries_.append({ s, included, true });
  }
  endResetModel();
  Q_EMIT changed();
}

QString
ImageSetModel::importFiles(const QList<QUrl>& files, const QString& name, int fit, const QColor& background)
{
  const QString setName = ImageTools::sanitizeSetName(name);
  if (!ImageTools::isValidSetName(setName))
    return QStringLiteral("Please enter a name (letters, digits and '-', at most 20 characters).");
  if (find(setName) != nullptr)
    return QStringLiteral("A set called '%1' already exists.").arg(setName);
  QStringList paths;
  for (const QUrl& u : files)
    paths << u.toLocalFile();
  QStringList errors;
  const ImageSet set = ImageSet::fromFiles(paths, setName, static_cast<ImageTools::Fit>(fit), background, &errors);
  if (!set.isComplete()) {
    errors.prepend(QStringLiteral("The set is incomplete: %1 missing. Name the files so they end in the digit "
                                  "(e.g. 0.png ... 9.png) or use <set>_<tube>_<digit>.png.")
                     .arg(set.missing().join(QStringLiteral(", "))));
    return errors.join(QLatin1Char('\n'));
  }
  beginInsertRows({}, int(entries_.size()), int(entries_.size()));
  entries_.append({ set, true, false });
  endInsertRows();
  Q_EMIT changed();
  return errors.join(QLatin1Char('\n')); // warnings for skipped files, if any
}

void
ImageSetModel::remove(int row)
{
  if (row < 0 || row >= entries_.size())
    return;
  beginRemoveRows({}, row, row);
  entries_.removeAt(row);
  endRemoveRows();
  Q_EMIT changed();
}

void
ImageSetModel::setIncluded(int row, bool included)
{
  if (row < 0 || row >= entries_.size() || entries_[row].included == included)
    return;
  entries_[row].included = included;
  Q_EMIT dataChanged(index(row), index(row), { IncludedRole });
  Q_EMIT changed();
}

qint64
ImageSetModel::includedBytes() const
{
  qint64 n = 0;
  for (const Entry& e : entries_)
    if (e.included)
      n += ClockFilesystem::estimateBytes(e.set);
  return n;
}

QStringList
ImageSetModel::includedNames() const
{
  QStringList n;
  for (const Entry& e : entries_)
    if (e.included)
      n << e.set.name();
  return n;
}

QList<ImageSet>
ImageSetModel::includedSets() const
{
  QList<ImageSet> out;
  for (const Entry& e : entries_)
    if (e.included)
      out << e.set;
  return out;
}

const ImageSet*
ImageSetModel::find(const QString& name) const
{
  for (const Entry& e : entries_)
    if (e.set.name() == name)
      return &e.set;
  return nullptr;
}

QImage
SetPreviewProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
  // <modelId>/<set>/<screen>/<digit>
  const QStringList parts = id.split(QLatin1Char('/'));
  QImage img;
  if (parts.size() == 4) {
    if (const ImageSetModel* m = ImageSetModel::byId(parts.at(0).toInt()))
      if (const ImageSet* s = m->find(parts.at(1)))
        img = QImage::fromData(s->resolved(parts.at(2).toInt(), parts.at(3).toInt()), "png");
  }
  if (img.isNull()) {
    img = QImage(ImageTools::kWidth, ImageTools::kHeight, QImage::Format_RGB32);
    img.fill(Qt::black);
  }
  if (size)
    *size = img.size();
  // QML often requests only one dimension (the other is 0).
  if (requestedSize.width() > 0 && requestedSize.height() > 0)
    img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  else if (requestedSize.height() > 0)
    img = img.scaledToHeight(requestedSize.height(), Qt::SmoothTransformation);
  else if (requestedSize.width() > 0)
    img = img.scaledToWidth(requestedSize.width(), Qt::SmoothTransformation);
  return img;
}
