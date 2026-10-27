#pragma once

#include <QQuickImageProvider>

// Serves an image with inverted colours: "image://inverted/" followed by a file: or qrc:
// URL. Night mode inverts everything the app draws, so the system apps pre-invert images
// and the inversion restores them: covers and photos keep their colours while the page goes
// dark (night.js picks the URL).
class InvertedImageProvider : public QQuickImageProvider
{
public:
    InvertedImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
