#include "ui/accents.h"

#include <QtMath>

#include <cmath>

namespace {

struct Accent {
    const char *name;
    double lightness;   // OKLCH, as BookOrbit's light theme shows it
    double chroma;
    double hue;
    bool pastel;        // light enough for dark text on it
};

// BookOrbit's accent palette (from the web client's accent definitions), light theme
const Accent ACCENTS[] = {
    {"acid-green", 0.7, 0.2, 112.0, false},
    {"aloe", 0.68, 0.09, 152.0, true},
    {"amber", 0.72, 0.17, 70.0, false},
    {"amethyst", 0.54, 0.24, 302.0, false},
    {"aqua", 0.7, 0.09, 188.0, true},
    {"baby-blue", 0.68, 0.12, 230.0, true},
    {"blue", 0.487, 0.25, 263.0, false},
    {"bluebell", 0.68, 0.125, 270.0, true},
    {"blush", 0.68, 0.13, 328.0, true},
    {"butter", 0.82, 0.085, 70.0, true},
    {"celadon", 0.76, 0.1, 118.0, true},
    {"chartreuse", 0.76, 0.2, 106.0, false},
    {"copper", 0.65, 0.19, 56.0, false},
    {"coral", 0.69, 0.12, 15.0, true},
    {"cornflower", 0.68, 0.14, 247.0, true},
    {"cyan", 0.52, 0.2, 197.0, false},
    {"electric-blue", 0.54, 0.24, 230.0, false},
    {"emerald", 0.52, 0.17, 162.0, false},
    {"flax", 0.82, 0.09, 82.0, true},
    {"foam", 0.68, 0.085, 171.0, true},
    {"fuchsia", 0.56, 0.27, 312.0, false},
    {"green", 0.527, 0.18, 142.0, false},
    {"grey", 0.55, 0.0, 0.0, true},
    {"indigo", 0.51, 0.26, 276.0, false},
    {"iris", 0.52, 0.25, 270.0, false},
    {"jade", 0.62, 0.2, 155.0, false},
    {"lavender", 0.68, 0.135, 292.0, true},
    {"lemon", 0.82, 0.09, 95.0, true},
    {"lime", 0.64, 0.2, 118.0, false},
    {"malachite", 0.56, 0.18, 152.0, false},
    {"marigold", 0.76, 0.18, 82.0, false},
    {"mauve", 0.68, 0.12, 302.0, true},
    {"mint", 0.68, 0.085, 162.0, true},
    {"orange", 0.64, 0.22, 42.0, false},
    {"orchid", 0.68, 0.135, 312.0, true},
    {"peach", 0.76, 0.11, 42.0, true},
    {"pear", 0.82, 0.1, 106.0, true},
    {"periwinkle", 0.68, 0.125, 263.0, true},
    {"pink", 0.56, 0.26, 328.0, false},
    {"pistachio", 0.8, 0.11, 112.0, true},
    {"powder", 0.68, 0.1, 197.0, true},
    {"purple", 0.51, 0.25, 284.0, false},
    {"raspberry", 0.56, 0.23, 344.0, false},
    {"rose", 0.57, 0.24, 15.0, false},
    {"rose-quartz", 0.68, 0.115, 344.0, true},
    {"rosewater", 0.7, 0.125, 2.0, true},
    {"sage", 0.68, 0.09, 142.0, true},
    {"salmon", 0.74, 0.12, 28.0, true},
    {"sand", 0.77, 0.095, 56.0, true},
    {"scarlet", 0.58, 0.25, 2.0, false},
    {"sea-glass", 0.74, 0.1, 155.0, true},
    {"seafoam", 0.68, 0.09, 180.0, true},
    {"sprout", 0.8, 0.095, 130.0, true},
    {"teal", 0.52, 0.18, 180.0, false},
    {"thistle", 0.68, 0.125, 284.0, true},
    {"turquoise", 0.58, 0.18, 188.0, false},
    {"ultramarine", 0.5, 0.28, 247.0, false},
    {"vermilion", 0.62, 0.24, 28.0, false},
    {"violet", 0.491, 0.27, 292.0, false},
    {"viridian", 0.54, 0.17, 171.0, false},
    {"wasabi", 0.68, 0.19, 130.0, false},
    {"white", 0.2, 0.0, 0.0, false},
    {"wisteria", 0.68, 0.13, 276.0, true},
    {"yellow", 0.75, 0.18, 95.0, false},
};

// OKLCH -> sRGB (https://bottosson.github.io/posts/oklab/), clipped to the sRGB gamut
QColor fromOklch(double lightness, double chroma, double hue)
{
    const double a = chroma * qCos(qDegreesToRadians(hue));
    const double b = chroma * qSin(qDegreesToRadians(hue));
    const double l = qPow(lightness + 0.3963377774 * a + 0.2158037573 * b, 3);
    const double m = qPow(lightness - 0.1055613458 * a - 0.0638541728 * b, 3);
    const double s = qPow(lightness - 0.0894841775 * a - 1.2914855480 * b, 3);
    const double linear[3] = {
        +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s,
    };
    double rgb[3];
    for (int i = 0; i < 3; i++) {
        const double c = qBound(0.0, linear[i], 1.0);
        rgb[i] = c <= 0.0031308 ? 12.92 * c : 1.055 * qPow(c, 1 / 2.4) - 0.055;
    }
    return QColor::fromRgbF(rgb[0], rgb[1], rgb[2]);
}

} // namespace

static const Accent *findAccent(const QString &name)
{
    for (const Accent &accent : ACCENTS) {
        if (name == QLatin1String(accent.name))
            return &accent;
    }
    return nullptr;
}

AccentColors accentColors(const QString &name)
{
    if (const Accent *accent = findAccent(name))
        return {fromOklch(accent->lightness, accent->chroma, accent->hue),
                accent->pastel ? QColor(Qt::black) : QColor(Qt::white)};
    return accentColors(QStringLiteral("teal"));   // BookOrbit's default
}

CoverPalette coverPalette(const QString &seed, const QString &accentName)
{
    const Accent *accent = findAccent(accentName);
    if (!accent)
        accent = findAccent(QStringLiteral("teal"));
    // djb2 over UTF-16 code units, matching JavaScript's charCodeAt
    quint32 n = 5381;
    for (const QChar c : seed)
        n = (n << 5) + n + c.unicode();
    // Within 36 degrees of the accent hue (its --tint-h), in steps of 12
    static const int hueOffsets[] = {-36, -24, -12, 0, 12, 24, 36};
    double hue = std::fmod(accent->hue + hueOffsets[n % 7], 360.0);
    if (hue < 0)
        hue += 360;
    hue = qRound(hue);
    const int n2 = (n >> 8) & 0xff;
    const int n3 = (n >> 16) & 0xff;
    // Paler and greyer for the pastel accents
    const bool pastel = accent->pastel;
    const double lightness = pastel ? 0.66 + (n2 % 5) * 0.03 : 0.45 + (n2 % 6) * 0.05;
    const double chroma = pastel ? 0.04 + (n3 % 4) * 0.012 : 0.12 + (n3 % 5) * 0.02;
    return {fromOklch(lightness, chroma, hue),
            fromOklch(lightness - (pastel ? 0.16 : 0.2), chroma, hue),
            fromOklch(0.99, 0.01, hue)};
}
