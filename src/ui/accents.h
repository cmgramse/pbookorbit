#pragma once

#include <QColor>
#include <QString>

// Colours of one of BookOrbit's accents (its "Accent color" setting)
struct AccentColors {
    QColor color;   // the accent itself
    QColor text;    // text and icons on it: black on pastel accents, otherwise white
};

// Colours for an accent name such as "teal" or "wisteria". Unknown names get BookOrbit's
// default, teal.
AccentColors accentColors(const QString &name);

// BookOrbit's colours for generated artwork such as an author's monogram: a diagonal
// gradient (top left to bottom right) and a text colour. Chosen by hashing seed (the
// author's name) among shades near the accent hue, like the web client's bookCoverPalette()
// in its light theme.
struct CoverPalette {
    QColor from;
    QColor to;
    QColor text;
};
CoverPalette coverPalette(const QString &seed, const QString &accent);
