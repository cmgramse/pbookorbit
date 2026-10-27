#include "ui/einkcolors.h"

#include <inkview.h>

void adjustForEink(QImage &image)
{
    // InkView works on 24-bit RGB rows (tested on the device: RGB order, any scanline)
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    adjustAreaDefault(rgb.bits(), int(rgb.bytesPerLine()), rgb.width(), rgb.height());
    image = rgb.convertToFormat(QImage::Format_RGB32);
}

QColor einkColor(const QColor &color)
{
    // Large enough for the dither pattern to average out
    QImage patch(16, 16, QImage::Format_RGB32);
    patch.fill(color);
    adjustForEink(patch);
    qint64 r = 0, g = 0, b = 0;
    for (int y = 0; y < patch.height(); y++) {
        const auto *line = reinterpret_cast<const QRgb *>(patch.constScanLine(y));
        for (int x = 0; x < patch.width(); x++) {
            r += qRed(line[x]);
            g += qGreen(line[x]);
            b += qBlue(line[x]);
        }
    }
    const int n = patch.width() * patch.height();
    return QColor(int(r / n), int(g / n), int(b / n), color.alpha());
}
