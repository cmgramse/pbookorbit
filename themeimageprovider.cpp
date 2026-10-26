#include "themeimageprovider.h"

#include "diagnostics.h"

#include <inkview.h>
#include <cstdlib>

// Some theme resources aren't images (layouts, for one) but GetResource still returns
// them as an ibitmap, with nonsense in the header. Accept only plausible bitmaps.
static bool looksLikeBitmap(const ibitmap *bmp)
{
    const int depth = bmp->depth & 0xff;
    const bool knownDepth = depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 24 || depth == 32;
    const int minScanline = (bmp->width * depth + 7) / 8;
    return knownDepth && bmp->width > 0 && bmp->height > 0 && bmp->width <= 4096 && bmp->height <= 4096
           && bmp->scanline >= minScanline && bmp->scanline <= minScanline + 8;
}

// Grey levels (1, 2, 4 or 8 bits, high bits first) or colour (24-bit R, G, B; 32-bit
// R, G, B, A). Bit 15 of depth means a 1-bit transparency mask follows the pixels.
// Checked against a PocketBook Era Color's theme, with the icons that pocketbook-kit's
// pb device import saves.
static QImage toImage(const ibitmap *bmp)
{
    const int depth = bmp->depth & 0xff;
    const bool hasMask = bmp->depth & 0x8000;
    const int maskScanline = (bmp->width + 7) / 8;
    const unsigned char *mask = bmp->data + bmp->height * bmp->scanline;
    QImage image(bmp->width, bmp->height, QImage::Format_ARGB32);
    for (int y = 0; y < bmp->height; ++y) {
        const unsigned char *row = bmp->data + bmp->scanline * y;
        QRgb *out = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < bmp->width; ++x) {
            QRgb pixel;
            if (depth <= 8) {
                const int bit = x * depth;
                const int value = (row[bit / 8] >> (8 - depth - bit % 8)) & ((1 << depth) - 1);
                const int grey = value * 255 / ((1 << depth) - 1);
                pixel = qRgb(grey, grey, grey);
            } else if (depth == 24) {
                pixel = qRgb(row[x * 3], row[x * 3 + 1], row[x * 3 + 2]);
            } else {
                pixel = qRgba(row[x * 4], row[x * 4 + 1], row[x * 4 + 2], row[x * 4 + 3]);
            }
            if (hasMask && !((mask[maskScanline * y + x / 8] >> (7 - x % 8)) & 1))
                pixel = qRgba(0, 0, 0, 0);
            out[x] = pixel;
        }
    }
    return image;
}

QImage ThemeImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    static const QString invertedSuffix = "@inverted";
    const bool inverted = id.endsWith(invertedSuffix);
    const QString name = inverted ? id.chopped(invertedSuffix.size()) : id;

    ibitmap *bmp = GetResource(name.toUtf8().constData(), nullptr);
    if (!bmp) {
        diagnostics::log("theme icon not found: " + name);
        return QImage();
    }
    if (!looksLikeBitmap(bmp)) {
        diagnostics::log("theme resource is not an image: " + name);
        free(bmp);
        return QImage();
    }
    QImage image = toImage(bmp);
    free(bmp);
    if (inverted)
        image.invertPixels(QImage::InvertRgb);

    if (size)
        *size = image.size();
    // Scaled only to a real size: QML sends 0x0 (valid, but empty) for "its own size"
    if (!requestedSize.isEmpty() && requestedSize != image.size())
        return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}
