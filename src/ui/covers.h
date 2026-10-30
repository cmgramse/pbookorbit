#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QString>

class BookOrbitClient;

// Cover thumbnails for book lists and photos for the authors list. Downloads a book's
// thumbnail (GET /books/:id/thumbnail, about 100 KB) or an author's (GET
// /authors/:id/thumbnail) once per version and keeps it as it came. Each display size is
// made from that: scaled (photos cropped to fill, as BookOrbit does), its colours adjusted
// for the screen (einkcolors.h), and cached as a PNG.
//
// QML calls source(). If the cover hasn't been downloaded, it returns empty, starts a
// download and emits ready(bookId) when done. authorPhoto() and photoReady(authorId) work
// the same way. Offline it doesn't download, and emits them once back online so QML asks
// again. Meanwhile it uses a download of another version, or rescales a copy cached at
// another size, if any.
class Covers : public QObject
{
    Q_OBJECT

public:
    Covers(BookOrbitClient *client, const QString &cacheDir, QObject *parent = nullptr);

    // File URL of the cached cover, or "" while downloading. version is the book's
    // coverVersion, so changed covers are fetched again.
    Q_INVOKABLE QString source(int bookId, const QString &version, int width, int height);
    // Same for an author's photo. imageUrl comes from the authors list and includes the
    // path and version, e.g. "/api/v1/authors/7/thumbnail?t=1788298235776"
    Q_INVOKABLE QString authorPhoto(int authorId, const QString &imageUrl, int width, int height);

signals:
    void ready(int bookId);
    void photoReady(int authorId);

private:
    struct Request {
        // A book cover or an author photo
        int bookId;
        int authorId;
        QString path;
        QSize size;
        QString name;   // see cacheFile()
        QString file;
        QString source;   // see sourceFile()
    };

    // name is "<bookId>" for a cover or "author-<authorId>" for a photo, and versionHash
    // is versionHash()'s
    QString cacheFile(const QString &name, const QString &versionHash, const QSize &size) const;
    // The download, as it came (JPEG or PNG)
    QString sourceFile(const QString &name, const QString &versionHash) const;
    // Makes the request's size from a download. Returns the file, or "" if it failed.
    QString sizeFrom(const Request &request, const QString &source);
    // Offline: the request's size, made from a download of any version or rescaled from
    // the largest copy cached, or "" if there's neither
    QString localCopy(const Request &request);
    // Cached file URL, or "" while downloading
    QString fetch(const Request &request);
    void startDownloads();
    // Notes a cover or photo to ask for again once online
    void missed(const Request &request);

    BookOrbitClient *m_client;
    QString m_cacheDir;
    // Newest requests first, so covers on screen beat ones already scrolled past
    QList<Request> m_queue;
    QSet<QString> m_wanted;
    int m_running = 0;
    // Wanted while offline
    QSet<int> m_missedBooks;
    QSet<int> m_missedAuthors;
    // Cached sizes and downloads by name (see cacheFile and sourceFile), read from the
    // folder when first needed offline and again after each time online
    QHash<QString, QStringList> m_cached;
    QHash<QString, QStringList> m_sources;
    bool m_cachedRead = false;
};
