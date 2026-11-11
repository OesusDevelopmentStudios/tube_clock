// SPDX-License-Identifier: GPL-3.0-or-later
#include <QBuffer>
#include <QImageReader>
#include <QTemporaryDir>
#include <QtTest>

#include "ImageSet.hpp"

using namespace ImageTools;

class TestImageSet : public QObject
{
    Q_OBJECT
private:
    static QImage decode(const QByteArray& png)
    {
        QBuffer b;
        b.setData(png);
        QImageReader r(&b, "png");
        return r.read();
    }

    static void writeImage(const QString& path, const QSize& size, const QColor& color)
    {
        QImage img(size, QImage::Format_RGB32);
        img.fill(color);
        QVERIFY(img.save(path));
    }

private Q_SLOTS:
    void prepareSizesAndRounds()
    {
        QImage wide(400, 100, QImage::Format_RGB32);
        wide.fill(QColor(0x12, 0x34, 0x56));
        const QImage contain = prepare(wide, Fit::Contain, Qt::black);
        QCOMPARE(contain.size(), QSize(kWidth, kHeight));
        QCOMPARE(QColor(contain.pixel(0, 0)), QColor(Qt::black)); // letterbox
        // Centre pixel rounded to RGB565 precision: 0x12->0x10(+rep), 0x34->0x34, 0x56->0x52(+rep)
        const QColor c(contain.pixel(kWidth / 2, kHeight / 2));
        QCOMPARE(c.red() >> 3, 0x12 >> 3);
        QCOMPARE(c.green() >> 2, 0x34 >> 2);
        QCOMPARE(c.blue() >> 3, 0x56 >> 3);
        const QImage cover = prepare(wide, Fit::Cover, Qt::black);
        QCOMPARE(QColor(cover.pixel(0, 0)).red() >> 3, 0x12 >> 3); // no border
    }

    void pngPaletteRoundTrip()
    {
        QImage img(kWidth, kHeight, QImage::Format_RGB888);
        img.fill(Qt::black);
        for (int y = 50; y < 150; ++y)
        {
            for (int x = 20; x < 100; ++x)
            {
                img.setPixelColor(x, y, QColor(248, 128, 0));
            }
        }

        const QByteArray png = encodePng(img);
        QVERIFY(png.startsWith("\x89PNG"));
        const QImage back = decode(png);
        QCOMPARE(back.size(), img.size());
        QVERIFY(back.format() == QImage::Format_Indexed8 || back.colorCount() > 0); // written as palette
        QCOMPARE(back.pixelColor(50, 100), QColor(248, 128, 0));
        QCOMPARE(back.pixelColor(0, 0), QColor(Qt::black));
    }

    void pngTrueColourWhenManyColours()
    {
        QImage img(kWidth, kHeight, QImage::Format_RGB888);
        for (int y = 0; y < kHeight; ++y)
        {
            for (int x = 0; x < kWidth; ++x)
            {
                img.setPixelColor(x, y, QColor(x, y, (x * y) % 256));
            }
        }

        const QImage back = decode(encodePng(img));
        QCOMPARE(back.pixelColor(77, 133), QColor(77, 133, (77 * 133) % 256));
    }

    void names()
    {
        ImageName n;
        QVERIFY(parseImageName(QStringLiteral("nixie_3_7.jpg"), n));
        QCOMPARE(n.set, QStringLiteral("nixie"));
        QCOMPARE(n.screen, 3);
        QCOMPARE(n.digit, 7);
        QVERIFY(parseImageName(QStringLiteral("/tmp/x/my-set_0_0.PNG"), n));
        QVERIFY(!parseImageName(QStringLiteral("Nixie_0_0.png"), n));
        QVERIFY(!parseImageName(QStringLiteral("nixie_6_0.png"), n));
        QCOMPARE(fileName(QStringLiteral("nixie"), 5, 9), QStringLiteral("nixie_5_9.png"));
        QCOMPARE(sanitizeSetName(QStringLiteral("  Blue Ribbon! v2 ")), QStringLiteral("blue-ribbon-v2"));
        QCOMPARE(sanitizeSetName(QStringLiteral("A very long name for a clock face")),
            QStringLiteral("a-very-long-name-for"));
        QVERIFY(isValidSetName(sanitizeSetName(QStringLiteral("___"))) == false);
    }

    void importOnePicturePerDigit()
    {
        QTemporaryDir dir;
        for (int d = 0; d < 10; ++d)
        {
            writeImage(dir.filePath(QStringLiteral("digit%1.png").arg(d)), QSize(270, 480), QColor(d * 20, 0, 0));
        }

        QStringList paths;
        for (int d = 0; d < 10; ++d)
        {
            paths << dir.filePath(QStringLiteral("digit%1.png").arg(d));
        }

        QStringList errors;
        const ImageSet set = ImageSet::fromFiles(paths, QStringLiteral("red"), Fit::Contain, Qt::black, &errors);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
        QVERIFY(set.isComplete());
        QCOMPARE(set.files().size(), 10);
        QVERIFY(set.files().contains(QStringLiteral("red_0_9.png")));
        // Tube 4 falls back to the screen 0 picture.
        QCOMPARE(set.resolved(4, 3), set.png(0, 3));
        QCOMPARE(decode(set.png(0, 5)).pixelColor(60, 120).red() >> 3, (5 * 20) >> 3);
    }

    void importPerTubePictures()
    {
        QTemporaryDir dir;
        QStringList paths;
        for (int d = 0; d < 10; ++d)
        {
            paths << dir.filePath(QStringLiteral("x_0_%1.jpg").arg(d));
            writeImage(paths.last(), QSize(135, 240), Qt::white);
        }

        paths << dir.filePath(QStringLiteral("x_5_1.png"));
        writeImage(paths.last(), QSize(135, 240), Qt::blue);
        paths << dir.filePath(QStringLiteral("readme.txt"));
        QFile readme(paths.last());
        QVERIFY(readme.open(QIODevice::WriteOnly));
        readme.close();
        QStringList errors;
        const ImageSet set = ImageSet::fromFiles(paths, QStringLiteral("mixed"), Fit::Contain, Qt::black, &errors);
        QCOMPARE(errors.size(), 1); // readme.txt
        QVERIFY(set.isComplete());
        QCOMPARE(set.files().size(), 11);
        QCOMPARE(decode(set.resolved(5, 1)).pixelColor(10, 10), QColor(Qt::blue));
    }

    void incompleteSet()
    {
        ImageSet s(QStringLiteral("half"));
        for (int d = 0; d < 5; ++d)
        {
            s.setPng(0, d, QByteArray("x"));
        }

        s.setPng(3, 7, QByteArray("x")); // does not help: screen 0 is the fallback
        QVERIFY(!s.isComplete());
        QCOMPARE(s.missing().size(), 5);
    }

    void bundledDefaultSets()
    {
        const QList<ImageSet> sets = ImageSet::fromClockFolder(QStringLiteral(REPO_ROOT "/data"));
        QCOMPARE(sets.size(), 7);
        for (const ImageSet& s : sets)
        {
            QVERIFY2(s.isComplete(), qPrintable(s.name()));
            QCOMPARE(decode(s.png(0, 0)).size(), QSize(kWidth, kHeight));
        }
    }
};

QTEST_MAIN(TestImageSet)
#include "test_imageset.moc"
