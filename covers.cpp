#include "covers.h"

#include "bookorbitclient.h"
#include "diagnostics.h"
#include "einkcolors.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

// Downloads at once: enough to fill a screen quickly without swamping the device
static const int MAX_DOWNLOADS = 3;

Covers::Covers(BookOrbitClient *client, const QString &cacheDir, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_cacheDir(cacheDir)
{
    QDir().mkpath(m_cacheDir);
}

QString Covers::cacheFile(int bookId, const QString &version, const QSize &size) const
{
    const QString versionHash = QString::fromLatin1(
        QCryptographicHash::hash(version.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
    // Adjusted for the screen and dithered, so PNG: JPEG would smear the dither
    return QString("%1/%2-%3-%4x%5.png").arg(m_cacheDir).arg(bookId).arg(versionHash)
        .arg(size.width()).arg(size.height());
}

QString Covers::source(int bookId, const QString &version, int width, int height)
{
    const QSize size(width, height);
    const QString file = cacheFile(bookId, version, size);
    if (QFileInfo::exists(file))
        return QUrl::fromLocalFile(file).toString();

    if (!m_wanted.contains(file)) {
        m_wanted.insert(file);
        m_queue.prepend({bookId, version, size, file});
        startDownloads();
    }
    return QString();
}

void Covers::startDownloads()
{
    while (m_running < MAX_DOWNLOADS && !m_queue.isEmpty()) {
        const Request request = m_queue.takeFirst();
        m_running++;
        m_client->download(QString("/books/%1/thumbnail").arg(request.bookId),
            [this, request](const QByteArray &data) {
                m_running--;
                m_wanted.remove(request.file);
                QImage image;
                if (image.loadFromData(data)) {
                    // Shrink to fit the space it is shown in, keeping its shape
                    image = image.scaled(request.size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    // Adjusted for the colour screen, as the system library shows covers
                    adjustForEink(image);
                    if (image.save(request.file, "PNG"))
                        emit ready(request.bookId);
                } else {
                    diagnostics::log(QString("cover %1: not an image").arg(request.bookId));
                }
                startDownloads();
            },
            [this, request](int status, const QString &message) {
                m_running--;
                m_wanted.remove(request.file);
                diagnostics::log(QString("cover %1: %2 (%3)").arg(request.bookId).arg(message).arg(status));
                startDownloads();
            });
    }
}
