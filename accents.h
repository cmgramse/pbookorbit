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
