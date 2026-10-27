#pragma once

#include <QQuickImageProvider>

// Serves an image with its colours inverted: "image://inverted/" followed by its file:
// or qrc: URL. In night mode the screen inverts everything the app draws, so the system's
// own apps draw pictures inverted for it to turn back: covers and photos keep their real
// colours while the rest of the page goes dark (night.js picks the URL).
class InvertedImageProvider : public QQuickImageProvider
{
public:
    InvertedImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
