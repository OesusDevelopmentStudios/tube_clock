// SPDX-License-Identifier: GPL-3.0-or-later
#include "LittleFsImage.h"

#include <cstring>

extern "C"
{
#include "lfs.h"
}

namespace {

// Same values as esp_littlefs in the ESP32 Arduino core 2.0.x (sdkconfig) and as
// PlatformIO's mklittlefs, so the clock mounts the image without reformatting.
constexpr lfs_size_t kReadSize = 128;
constexpr lfs_size_t kProgSize = 128;
constexpr lfs_size_t kCacheSize = 512;
constexpr lfs_size_t kLookahead = 128;
constexpr int32_t kBlockCycles = 512;
constexpr lfs_size_t kNameMax = 32;
constexpr uint32_t kDiskVersion = 0x00020000; // littlefs 2.0 on-disk format

struct Storage
{
  uint8_t* data;
  lfs_size_t blockSize;
};

int
bdRead(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size)
{
  auto* s = static_cast<Storage*>(c->context);
  std::memcpy(buffer, s->data + size_t(block) * s->blockSize + off, size);
  return LFS_ERR_OK;
}

int
bdProg(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size)
{
  auto* s = static_cast<Storage*>(c->context);
  // NOR flash semantics: programming can only clear bits.
  uint8_t* dst = s->data + size_t(block) * s->blockSize + off;
  const auto* src = static_cast<const uint8_t*>(buffer);
  for (lfs_size_t i = 0; i < size; ++i)
    dst[i] &= src[i];
  return LFS_ERR_OK;
}

int
bdErase(const struct lfs_config* c, lfs_block_t block)
{
  auto* s = static_cast<Storage*>(c->context);
  std::memset(s->data + size_t(block) * s->blockSize, 0xFF, s->blockSize);
  return LFS_ERR_OK;
}

int
bdSync(const struct lfs_config*)
{
  return LFS_ERR_OK;
}

void
fillConfig(lfs_config& cfg, Storage* storage, lfs_size_t blockCount)
{
  std::memset(&cfg, 0, sizeof(cfg));
  cfg.context = storage;
  cfg.read = bdRead;
  cfg.prog = bdProg;
  cfg.erase = bdErase;
  cfg.sync = bdSync;
  cfg.read_size = kReadSize;
  cfg.prog_size = kProgSize;
  cfg.block_size = LittleFsImage::kBlockSize;
  cfg.block_count = blockCount;
  cfg.block_cycles = kBlockCycles;
  cfg.cache_size = kCacheSize;
  cfg.lookahead_size = kLookahead;
  cfg.name_max = kNameMax;
  cfg.disk_version = kDiskVersion;
}

QString
lfsError(int err)
{
  return QStringLiteral("littlefs error %1").arg(err);
}

} // namespace

struct LittleFsImage::Impl
{
  Storage storage{};
  lfs_config cfg{};
  lfs_t lfs{};
};

LittleFsImage::LittleFsImage(quint32 sizeBytes)
  : storage_(int(sizeBytes), char(0xFF))
  , d_(new Impl)
{
  d_->storage = { reinterpret_cast<uint8_t*>(storage_.data()), kBlockSize };
  fillConfig(d_->cfg, &d_->storage, sizeBytes / kBlockSize);
}

LittleFsImage::~LittleFsImage()
{
  if (mounted_)
    lfs_unmount(&d_->lfs);
  delete d_;
}

bool
LittleFsImage::format(QString* error)
{
  int err = lfs_format(&d_->lfs, &d_->cfg);
  if (err == 0)
    err = lfs_mount(&d_->lfs, &d_->cfg);
  if (err != 0) {
    if (error)
      *error = lfsError(err);
    return false;
  }
  mounted_ = true;
  return true;
}

bool
LittleFsImage::addFile(const QString& name, const QByteArray& data, QString* error)
{
  if (!mounted_) {
    if (error)
      *error = QStringLiteral("filesystem not formatted");
    return false;
  }
  const QByteArray path = name.toUtf8();
  if (path.isEmpty() || path.size() > qsizetype(kNameMax) || path.contains('/')) {
    if (error)
      *error = QStringLiteral("invalid file name '%1'").arg(name);
    return false;
  }
  lfs_file_t file;
  int err = lfs_file_open(&d_->lfs, &file, path.constData(), LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
  if (err < 0) {
    if (error)
      *error = lfsError(err);
    return false;
  }
  const lfs_ssize_t written = lfs_file_write(&d_->lfs, &file, data.constData(), lfs_size_t(data.size()));
  err = lfs_file_close(&d_->lfs, &file);
  if (written != lfs_ssize_t(data.size()) || err < 0) {
    if (error)
      *error = written == LFS_ERR_NOSPC ? QStringLiteral("not enough space on the clock")
                                        : lfsError(written < 0 ? int(written) : err);
    lfs_remove(&d_->lfs, path.constData());
    return false;
  }
  return true;
}

quint64
LittleFsImage::usedBytes() const
{
  if (!mounted_)
    return 0;
  const lfs_ssize_t blocks = lfs_fs_size(&d_->lfs);
  return blocks < 0 ? 0 : quint64(blocks) * kBlockSize;
}

QByteArray
LittleFsImage::finish()
{
  if (mounted_) {
    lfs_unmount(&d_->lfs);
    mounted_ = false;
  }
  return storage_;
}

QMap<QString, QByteArray>
LittleFsImage::readAll(const QByteArray& image, QString* error)
{
  QMap<QString, QByteArray> out;
  QByteArray copy = image; // littlefs needs writable storage even for reading
  Storage storage{ reinterpret_cast<uint8_t*>(copy.data()), kBlockSize };
  lfs_config cfg;
  fillConfig(cfg, &storage, lfs_size_t(copy.size()) / kBlockSize);
  lfs_t lfs;
  int err = lfs_mount(&lfs, &cfg);
  if (err != 0) {
    if (error)
      *error = lfsError(err);
    return out;
  }
  lfs_dir_t dir;
  if (lfs_dir_open(&lfs, &dir, "/") == 0) {
    lfs_info info;
    while (lfs_dir_read(&lfs, &dir, &info) > 0) {
      if (info.type != LFS_TYPE_REG)
        continue;
      lfs_file_t f;
      if (lfs_file_open(&lfs, &f, info.name, LFS_O_RDONLY) < 0)
        continue;
      QByteArray data(int(info.size), Qt::Uninitialized);
      lfs_file_read(&lfs, &f, data.data(), info.size);
      lfs_file_close(&lfs, &f);
      out.insert(QString::fromUtf8(info.name), data);
    }
    lfs_dir_close(&lfs, &dir);
  }
  lfs_unmount(&lfs);
  return out;
}
