#include "ui/covers.h"

#include "core/bookorbitclient.h"
#include "core/diagnostics.h"
#include "ui/einkcolors.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QRegularExpression>
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
        if (!m_client->online()) {
            m_cachedRead = false;
            return;
        }
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

static QString versionHash(const QString &version)
{
    return QString::fromLatin1(QCryptographicHash::hash(version.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
}

QString Covers::cacheFile(const QString &name, const QString &versionHash, const QSize &size) const
{
    // PNG, since JPEG would smear the dithering
    return QString("%1/%2-%3-%4x%5.png").arg(m_cacheDir, name, versionHash)
        .arg(size.width()).arg(size.height());
}

QString Covers::sourceFile(const QString &name, const QString &versionHash) const
{
    return QString("%1/%2-%3-source").arg(m_cacheDir, name, versionHash);
}

// The parts of a cache file name, "<name>-<version hash>-<width>x<height>.png", and of a
// download's, "<name>-<version hash>-source"
static const QRegularExpression cacheName(R"(^(.+)-([0-9a-f]{12})-(\d+)x(\d+)\.png$)");
static const QRegularExpression sourceName(R"(^(.+)-([0-9a-f]{12})-source$)");

// Scaled to its display size: photos fill their space like BookOrbit's portraits
// (object-fit: cover; object-position: 50% 26%), cropping the sides equally and the top
// and bottom roughly 1:3, since faces are usually high in a portrait. Covers fit it,
// keeping their aspect ratio.
static QImage fitted(const QImage &image, const QSize &size, bool photo)
{
    if (!photo)
        return image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QImage scaled = image.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    return scaled.copy((scaled.width() - size.width()) / 2, qRound((scaled.height() - size.height()) * 0.26),
                       size.width(), size.height());
}

QString Covers::source(int bookId, const QString &version, int width, int height)
{
    const QSize size(width, height);
    const QString name = QString::number(bookId);
    const QString hash = versionHash(version);
    return fetch({bookId, -1, QString("/books/%1/thumbnail").arg(bookId), size, name,
                  cacheFile(name, hash, size), sourceFile(name, hash)});
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
    const QString name = QString("author-%1").arg(authorId);
    const QString hash = versionHash(version);
    return fetch({-1, authorId, path, size, name, cacheFile(name, hash, size), sourceFile(name, hash)});
}

QString Covers::sizeFrom(const Request &request, const QString &source)
{
    QImage image;
    if (!image.load(source)) {
        // Fetched again next time
        diagnostics::log(source + ": not an image");
        QFile::remove(source);
        return QString();
    }
    image = fitted(image, request.size, request.authorId >= 0);
    // Adjust for the colour screen, as the system library does for covers
    adjustForEink(image);
    if (!image.save(request.file, "PNG"))
        return QString();
    if (m_cachedRead)
        m_cached[request.name].append(QFileInfo(request.file).fileName());
    return request.file;
}

QString Covers::localCopy(const Request &request)
{
    if (!m_cachedRead) {
        m_cached.clear();
        m_sources.clear();
        for (const QString &fileName : QDir(m_cacheDir).entryList(QDir::Files)) {
            if (const QRegularExpressionMatch match = cacheName.match(fileName); match.hasMatch())
                m_cached[match.captured(1)].append(fileName);
            else if (const QRegularExpressionMatch source = sourceName.match(fileName); source.hasMatch())
                m_sources[source.captured(1)].append(fileName);
        }
        m_cachedRead = true;
    }
    // A download of another version, named for that version so the current one is still
    // fetched once online
    for (const QString &fileName : m_sources.value(request.name)) {
        const QString version = sourceName.match(fileName).captured(2);
        Request older = request;
        older.file = cacheFile(request.name, version, request.size);
        if (QFileInfo::exists(older.file))
            return older.file;
        const QString file = sizeFrom(older, m_cacheDir + '/' + fileName);
        if (!file.isEmpty())
            return file;
    }
    // One at this size from an earlier call, or else the largest to scale down
    QString largest;
    qint64 largestArea = 0;
    for (const QString &fileName : m_cached.value(request.name)) {
        const QRegularExpressionMatch match = cacheName.match(fileName);
        const QSize size(match.captured(3).toInt(), match.captured(4).toInt());
        if (size == request.size)
            return m_cacheDir + '/' + fileName;
        if (qint64(size.width()) * size.height() > largestArea) {
            largest = fileName;
            largestArea = qint64(size.width()) * size.height();
        }
    }
    QImage image;
    if (largest.isEmpty() || !image.load(m_cacheDir + '/' + largest))
        return QString();
    // Already adjusted for the screen. Named for the version it came from, so the current
    // one is still fetched once online.
    const QString file = cacheFile(request.name, cacheName.match(largest).captured(2), request.size);
    if (!fitted(image, request.size, request.authorId >= 0).save(file, "PNG"))
        return QString();
    m_cached[request.name].append(QFileInfo(file).fileName());
    return file;
}

QString Covers::fetch(const Request &request)
{
    // Not laid out yet: asked again once it has a size
    if (request.size.isEmpty())
        return QString();
    if (QFileInfo::exists(request.file))
        return QUrl::fromLocalFile(request.file).toString();
    // Downloaded already, for another size
    if (QFileInfo::exists(request.source)) {
        const QString file = sizeFrom(request, request.source);
        return file.isEmpty() ? QString() : QUrl::fromLocalFile(file).toString();
    }
    if (!m_client->online()) {
        missed(request);
        const QString file = localCopy(request);
        return file.isEmpty() ? QString() : QUrl::fromLocalFile(file).toString();
    }
    // One download however many sizes want it
    if (!m_wanted.contains(request.source)) {
        m_wanted.insert(request.source);
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
                m_wanted.remove(request.source);
                // Kept as it came; QML asks again for its size, which is made from it
                QBuffer buffer;
                buffer.setData(data);
                QFile file(request.source);
                if (!QImageReader(&buffer).canRead()) {
                    diagnostics::log(request.path + ": not an image");
                } else if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
                    diagnostics::log(request.source + ": " + file.errorString());
                    file.remove();
                } else {
                    file.close();
                    if (m_cachedRead)
                        m_sources[request.name].append(QFileInfo(request.source).fileName());
                    if (request.authorId >= 0)
                        emit photoReady(request.authorId);
                    else
                        emit ready(request.bookId);
                }
                startDownloads();
            },
            [this, request](int status, const QString &message) {
                m_running--;
                m_wanted.remove(request.source);
                if (!m_client->online())
                    missed(request);
                else
                    diagnostics::log(QString("%1: %2 (%3)").arg(request.path, message).arg(status));
                startDownloads();
            });
    }
}
