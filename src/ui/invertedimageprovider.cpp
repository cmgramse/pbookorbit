#include "ui/invertedimageprovider.h"

#include "core/diagnostics.h"

#include <QImageReader>
#include <QUrl>

QImage InvertedImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QUrl url(id);
    const QString path = url.scheme() == "qrc" ? ":" + url.path() : url.toLocalFile();
    QImageReader reader(path);
    // As with Image's sourceSize, a 0 side keeps the aspect ratio
    if (requestedSize.width() > 0 || requestedSize.height() > 0) {
        QSize scaled = reader.size();
        if (scaled.isValid())
            reader.setScaledSize(scaled.scaled(requestedSize.width() > 0 ? requestedSize.width() : scaled.width(),
                                               requestedSize.height() > 0 ? requestedSize.height() : scaled.height(),
                                               Qt::KeepAspectRatio));
    }
    QImage image = reader.read();
    if (image.isNull()) {
        diagnostics::log("can't load image to invert: " + id + ": " + reader.errorString());
        return image;
    }

    // Not premultiplied, so the inverted colours blend with the inverted page exactly as
    // the real ones would with the real page
    image = image.convertToFormat(image.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    image.invertPixels(QImage::InvertRgb);
    if (size)
        *size = image.size();
    return image;
}
