// SPDX-License-Identifier: GPL-3.0-or-later
//
// Image sets for the clock: conversion of arbitrary pictures into the 135x240 PNG
// files the firmware expects (<set>_<screen>_<digit>.png, see docs/image-sets.md).
#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QMap>
#include <QString>
#include <QStringList>

namespace ImageTools
{

constexpr int kWidth = 135;
constexpr int kHeight = 240;
constexpr int kScreens = 6;
constexpr int kDigits = 10;

enum class Fit
{
    Contain, // whole picture visible, borders filled with the background colour
    Cover,   // fills the tube, edges are cropped
    Stretch, // distorts to 135x240
};

// Scales/crops to 135x240 and rounds the colours to RGB565 (what the display shows),
// which makes the PNG files smaller without any visible change on the clock.
QImage prepare(const QImage& source, Fit fit = Fit::Contain, const QColor& background = Qt::black);

// Encodes as PNG; images with up to 256 colours are written as palette PNGs.
QByteArray encodePng(const QImage& image);

bool isValidSetName(const QString& name);
// Lower-cases and replaces invalid characters: "Blue Ribbon!" -> "blue-ribbon".
QString sanitizeSetName(const QString& name);

struct ImageName
{
    QString set;
    int screen = 0;
    int digit = 0;
};
// "<set>_<screen>_<digit>" with any (or no) extension, e.g. "nixie_0_7.jpg".
bool parseImageName(const QString& fileName, ImageName& out);
QString fileName(const QString& set, int screen, int digit); // "<set>_<s>_<d>.png"

} // namespace ImageTools

// A converted image set, ready to be written to the clock.
class ImageSet
{
public:
    ImageSet() = default;
    explicit ImageSet(const QString& name)
      : name_(name)
    {
    }

    QString name() const { return name_; }
    void setName(const QString& name) { name_ = name; }

    // png must already be a prepared 135x240 PNG.
    void setPng(int screen, int digit, const QByteArray& png);
    QByteArray png(int screen, int digit) const; // without fallback
    bool has(int screen, int digit) const { return files_.contains(key(screen, digit)); }
    // Image shown on the tube, with the firmware's fallback to screen 0.
    QByteArray resolved(int screen, int digit) const;
    bool isComplete() const;
    QStringList missing() const; // e.g. "screen 0, digit 7"
    qint64 totalBytes() const;
    // File name -> PNG data
    QMap<QString, QByteArray> files() const;

    // Imports a folder (or list of files) of pictures:
    //  - files named <set>_<screen>_<digit>.<ext>   (per-tube images), or
    //  - files whose name ends in a digit 0-9 (one picture per digit for all tubes).
    static ImageSet fromFiles(const QStringList& paths,
        const QString& setName,
        ImageTools::Fit fit,
        const QColor& background,
        QStringList* errors);
    // Reads a set that is already in the clock format (e.g. the bundled default sets).
    static QList<ImageSet> fromClockFolder(const QString& dir);

private:
    static int key(int screen, int digit) { return screen * 10 + digit; }
    QString name_;
    QMap<int, QByteArray> files_;
};
