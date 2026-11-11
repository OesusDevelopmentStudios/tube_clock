// SPDX-License-Identifier: GPL-3.0-or-later
#include "ImageSet.hpp"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>

#include "clockcore/ImageSet.hpp"

namespace ImageTools
{

QImage prepare(const QImage& source, Fit fit, const QColor& background)
{
    QImage canvas(kWidth, kHeight, QImage::Format_RGB888);
    canvas.fill(background);
    if (!source.isNull())
    {
        QSize target;
        switch (fit)
        {
            case Fit::Contain:
                target = source.size().scaled(kWidth, kHeight, Qt::KeepAspectRatio);
                break;
            case Fit::Cover:
                target = source.size().scaled(kWidth, kHeight, Qt::KeepAspectRatioByExpanding);
                break;
            case Fit::Stretch:
                target = QSize(kWidth, kHeight);
                break;
        }

        const QImage scaled = source.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QPainter p(&canvas);
        p.drawImage((kWidth - scaled.width()) / 2, (kHeight - scaled.height()) / 2, scaled);
    }

    // Round to RGB565 precision and expand back to 8 bit per channel.
    for (int y = 0; y < kHeight; ++y)
    {
        uchar* line = canvas.scanLine(y);
        for (int x = 0; x < kWidth; ++x)
        {
            uchar* px = line + x * 3;
            const int r = px[0] >> 3, g = px[1] >> 2, b = px[2] >> 3;
            px[0] = uchar((r << 3) | (r >> 2));
            px[1] = uchar((g << 2) | (g >> 4));
            px[2] = uchar((b << 3) | (b >> 2));
        }
    }

    return canvas;
}

QByteArray encodePng(const QImage& image)
{
    QImage img = image.convertToFormat(QImage::Format_RGB888);
    // Collect the colours (stop at 257: then a palette is not possible).
    QList<QRgb> palette;
    QHash<QRgb, int> index;
    for (int y = 0; y < img.height() && palette.size() <= 256; ++y)
    {
        const uchar* line = img.constScanLine(y);
        for (int x = 0; x < img.width(); ++x)
        {
            const QRgb c = qRgb(line[x * 3], line[x * 3 + 1], line[x * 3 + 2]);
            if (!index.contains(c))
            {
                index.insert(c, int(palette.size()));
                palette.append(c);
                if (palette.size() > 256)
                {
                    break;
                }
            }
        }
    }

    QImage out = img;
    if (palette.size() <= 256)
    {
        out = QImage(img.size(), QImage::Format_Indexed8);
        out.setColorTable(palette);
        for (int y = 0; y < img.height(); ++y)
        {
            const uchar* src = img.constScanLine(y);
            uchar* dst = out.scanLine(y);
            for (int x = 0; x < img.width(); ++x)
            {
                dst[x] = uchar(index.value(qRgb(src[x * 3], src[x * 3 + 1], src[x * 3 + 2])));
            }
        }
    }

    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    QImageWriter writer(&buf, "png");
    writer.setCompression(9);
    writer.setQuality(0); // Qt's PNG writer: 0 = smallest file (max zlib level)
    writer.write(out);
    return data;
}

bool isValidSetName(const QString& name)
{
    return clockcore::isValidSetName(name.toStdString());
}

QString sanitizeSetName(const QString& name)
{
    QString s = name.toLower();
    s.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    while (s.startsWith(QLatin1Char('-')))
    {
        s.remove(0, 1);
    }

    s.truncate(int(clockcore::kMaxSetNameLength));
    while (s.endsWith(QLatin1Char('-')))
    {
        s.chop(1);
    }

    return s;
}

bool parseImageName(const QString& file, ImageName& out)
{
    const QString base = QFileInfo(file).completeBaseName();
    // The clock only accepts ".png"; reuse its parser on the normalised name.
    clockcore::ImageName n;
    if (!clockcore::parseImageFileName((base + QStringLiteral(".png")).toStdString(), n))
    {
        return false;
    }

    out.set = QString::fromStdString(n.set);
    out.screen = n.screen;
    out.digit = n.digit;
    return true;
}

QString fileName(const QString& set, int screen, int digit)
{
    return QString::fromStdString(clockcore::imageFileName(set.toStdString(), screen, digit));
}

} // namespace ImageTools

using namespace ImageTools;

void ImageSet::setPng(int screen, int digit, const QByteArray& png)
{
    files_.insert(key(screen, digit), png);
}

QByteArray ImageSet::png(int screen, int digit) const
{
    return files_.value(key(screen, digit));
}

QByteArray ImageSet::resolved(int screen, int digit) const
{
    if (has(screen, digit))
    {
        return png(screen, digit);
    }

    return png(0, digit);
}

bool ImageSet::isComplete() const
{
    return missing().isEmpty();
}

QStringList ImageSet::missing() const
{
    // The firmware falls back to screen 0, so the set is complete exactly when the
    // screen 0 image exists for every digit.
    QStringList m;
    for (int d = 0; d < kDigits; ++d)
    {
        if (!has(0, d))
        {
            m << QStringLiteral("digit %1").arg(d);
        }
    }

    return m;
}

qint64 ImageSet::totalBytes() const
{
    qint64 n = 0;
    for (const QByteArray& b : files_)
    {
        n += b.size();
    }

    return n;
}

QMap<QString, QByteArray> ImageSet::files() const
{
    QMap<QString, QByteArray> out;
    for (auto it = files_.cbegin(); it != files_.cend(); ++it)
    {
        out.insert(fileName(name_, it.key() / 10, it.key() % 10), it.value());
    }

    return out;
}

ImageSet ImageSet::fromFiles(const QStringList& paths,
    const QString& setName,
    Fit fit,
    const QColor& background,
    QStringList* errors)
{
    ImageSet set(setName);
    static const QRegularExpression trailingDigit(QStringLiteral("(\\d)$"));
    QSet<int> seen;
    for (const QString& path : paths)
    {
        const QFileInfo fi(path);
        int screen = 0, digit = -1;
        ImageName n;
        if (parseImageName(fi.fileName(), n))
        {
            screen = n.screen;
            digit = n.digit;
        }
        else
        {
            const auto m = trailingDigit.match(fi.completeBaseName());
            if (m.hasMatch())
            {
                digit = m.captured(1).toInt();
            }
        }

        if (digit < 0)
        {
            if (errors)
            {
                errors->append(QStringLiteral("%1: the name does not tell which digit it is").arg(fi.fileName()));
            }

            continue;
        }

        if (seen.contains(screen * 10 + digit))
        {
            if (errors)
            {
                errors->append(QStringLiteral("%1: more than one picture for tube %2, digit %3")
                        .arg(fi.fileName())
                        .arg(screen + 1)
                        .arg(digit));
            }

            continue;
        }

        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QImage img = reader.read();
        if (img.isNull())
        {
            if (errors)
            {
                errors->append(QStringLiteral("%1: %2").arg(fi.fileName(), reader.errorString()));
            }

            continue;
        }

        seen.insert(screen * 10 + digit);
        set.setPng(screen, digit, encodePng(prepare(img, fit, background)));
    }

    return set;
}

QList<ImageSet> ImageSet::fromClockFolder(const QString& dir)
{
    QMap<QString, ImageSet> sets;
    const QDir d(dir);
    for (const QFileInfo& fi : d.entryInfoList({ QStringLiteral("*.png") }, QDir::Files, QDir::Name))
    {
        ImageName n;
        if (!parseImageName(fi.fileName(), n))
        {
            continue;
        }

        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
        {
            continue;
        }

        ImageSet& set = sets[n.set];
        set.setName(n.set);
        set.setPng(n.screen, n.digit, f.readAll());
    }

    return sets.values();
}
