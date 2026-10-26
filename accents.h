#pragma once

#include <QColor>
#include <QString>

// The colours of one of BookOrbit's accents (its "Accent color" appearance setting)
struct AccentColors {
    QColor color;   // the accent itself
    QColor text;    // for text and icons on it: black on the pastel accents, else white
};

// For an accent name such as "teal" or "wisteria"; BookOrbit's default, teal, for any
// name it doesn't know
AccentColors accentColors(const QString &name);

// BookOrbit's colours for artwork it makes up, such as an author's monogram when they
// have no photo: a diagonal gradient from one colour to the other, top left to bottom
// right, and the colour for text on it. Picked by a hash of seed (the author's name)
// from shades near the accent's hue, as the web client's bookCoverPalette() does for
// its light theme.
struct CoverPalette {
    QColor from;
    QColor to;
    QColor text;
};
CoverPalette coverPalette(const QString &seed, const QString &accent);
