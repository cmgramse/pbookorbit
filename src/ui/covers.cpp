#include "ui/covers.h"

#include "core/bookorbitclient.h"
#include "core/diagnostics.h"
#include "ui/einkcolors.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

// Concurrent downloads: enough to fill a screen quickly without swamping the device
static const int MAX_DOWNLOADS = 3;

Covers::Covers(BookOrbitClient *client, const QString &cacheDir, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_cacheDir(cacheDir)
{
    QDir().mkpath(m_cacheDir);
    connect(m_client, &BookOrbitClient::onlineChanged, this, [this]() {
        if (!m_client->online())
            return;
        for (int bookId : std::exchange(m_missedBooks, {}))
            emit ready(bookId);
        for (int authorId : std::exchange(m_missedAuthors, {}))
            emit photoReady(authorId);
    });
}

void Covers::missed(const Request &request)
{
    if (request.authorId >= 0)
        m_missedAuthors.insert(request.authorId);
    else
        m_missedBooks.insert(request.bookId);
}

QString Covers::cacheFile(const QString &name, const QString &version, const QSize &size) const
{
    const QString versionHash = QString::fromLatin1(
        QCryptographicHash::hash(version.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
    // PNG, since JPEG would smear the dithering
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
    // The path under the API (download() adds the prefix). The query is the version.
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
    if (!m_client->online()) {
        missed(request);
        return QString();
    }
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
                        // Photos fill their space like BookOrbit's portraits (object-fit:
                        // cover; object-position: 50% 26%): crop the sides equally, and the
                        // top and bottom roughly 1:3, since faces are usually high in a
                        // portrait
                        image = image.scaled(request.size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                        image = image.copy((image.width() - request.size.width()) / 2,
                                           qRound((image.height() - request.size.height()) * 0.26),
                                           request.size.width(), request.size.height());
                    } else {
                        // Scale to fit the display size, keeping the aspect ratio
                        image = image.scaled(request.size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    // Adjust for the colour screen, as the system library does for covers
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
                if (!m_client->online())
                    missed(request);
                else
                    diagnostics::log(QString("%1: %2 (%3)").arg(request.path, message).arg(status));
                startDownloads();
            });
    }
}
