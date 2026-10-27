#include "ui/covers.h"

#include "core/bookorbitclient.h"
#include "core/diagnostics.h"
#include "ui/einkcolors.h"

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

QString Covers::cacheFile(const QString &name, const QString &version, const QSize &size) const
{
    const QString versionHash = QString::fromLatin1(
        QCryptographicHash::hash(version.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
    // Adjusted for the screen and dithered, so PNG: JPEG would smear the dither
    return QString("%1/%2-%3-%4x%5.png").arg(m_cacheDir, name, versionHash)
        .arg(size.width()).arg(size.height());
}

QString Covers::source(int bookId, const QString &version, int width, int height)
{
    const QSize size(width, height);
    return fetch({bookId, -1, QString("/books/%1/thumbnail").arg(bookId), size,
                  cacheFile(QString::number(bookId), version, size)});
}

QString Covers::authorPhoto(int authorId, const QString &imageUrl, int width, int height)
{
    // The URL's path under the API, which download() adds; its query is the version
    static const QString apiPrefix = "/api/v1";
    if (!imageUrl.startsWith(apiPrefix))
        return QString();
    const QString path = imageUrl.mid(apiPrefix.size());
    const QString version = QUrl(imageUrl).query();
    const QSize size(width, height);
    return fetch({-1, authorId, path, size, cacheFile(QString("author-%1").arg(authorId), version, size)});
}

QString Covers::fetch(const Request &request)
{
    if (QFileInfo::exists(request.file))
        return QUrl::fromLocalFile(request.file).toString();

    if (!m_wanted.contains(request.file)) {
        m_wanted.insert(request.file);
        m_queue.prepend(request);
        startDownloads();
    }
    return QString();
}

void Covers::startDownloads()
{
    while (m_running < MAX_DOWNLOADS && !m_queue.isEmpty()) {
        const Request request = m_queue.takeFirst();
        m_running++;
        m_client->download(request.path,
            [this, request](const QByteArray &data) {
                m_running--;
                m_wanted.remove(request.file);
                QImage image;
                if (image.loadFromData(data)) {
                    if (request.authorId >= 0) {
                        // A photo fills its space, as BookOrbit's portraits do (object-fit:
                        // cover; object-position: 50% 26%): what doesn't fit is cut from
                        // the sides equally, and from the top and bottom about 1 to 3,
                        // as faces are usually in the upper part of a portrait
                        image = image.scaled(request.size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                        image = image.copy((image.width() - request.size.width()) / 2,
                                           qRound((image.height() - request.size.height()) * 0.26),
                                           request.size.width(), request.size.height());
                    } else {
                        // Shrink to fit the space it is shown in, keeping its shape
                        image = image.scaled(request.size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    // Adjusted for the colour screen, as the system library shows covers
                    adjustForEink(image);
                    if (image.save(request.file, "PNG")) {
                        if (request.authorId >= 0)
                            emit photoReady(request.authorId);
                        else
                            emit ready(request.bookId);
                    }
                } else {
                    diagnostics::log(request.path + ": not an image");
                }
                startDownloads();
            },
            [this, request](int status, const QString &message) {
                m_running--;
                m_wanted.remove(request.file);
                diagnostics::log(QString("%1: %2 (%3)").arg(request.path, message).arg(status));
                startDownloads();
            });
    }
}
