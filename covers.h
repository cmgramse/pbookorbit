#pragma once

#include <QList>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QString>

class BookOrbitClient;

// Cover thumbnails for the book list, and authors' photos for the authors list. Downloads
// each book's thumbnail from BookOrbit (GET /books/:id/thumbnail, about 100 KB), or an
// author's (GET /authors/:id/thumbnail), shrinks it to the size it is shown at and
// adjusts its colours for the screen (einkcolors.h) and keeps it as a PNG on the device,
// so it loads straight from storage next time.
//
// QML asks for a cover with source(); if it isn't cached yet the result is empty, a
// download starts, and ready(bookId) follows once source() has it. authorPhoto() and
// photoReady(authorId) do the same for a photo.
class Covers : public QObject
{
    Q_OBJECT

public:
    Covers(BookOrbitClient *client, const QString &cacheDir, QObject *parent = nullptr);

    // File URL of the cached cover, or "" while it downloads. version is the book's
    // coverVersion, so a changed cover is fetched again.
    Q_INVOKABLE QString source(int bookId, const QString &version, int width, int height);
    // The same for an author's photo; imageUrl is the authors list's, which says where it
    // is and which version: "/api/v1/authors/7/thumbnail?t=1788298235776"
    Q_INVOKABLE QString authorPhoto(int authorId, const QString &imageUrl, int width, int height);

signals:
    void ready(int bookId);
    void photoReady(int authorId);

private:
    struct Request {
        // A book's cover, or an author's photo
        int bookId;
        int authorId;
        QString path;
        QSize size;
        QString file;
    };

    // name: "<bookId>" for a cover, "author-<authorId>" for a photo
    QString cacheFile(const QString &name, const QString &version, const QSize &size) const;
    // The cached file's URL, or "" while it downloads
    QString fetch(const Request &request);
    void startDownloads();

    BookOrbitClient *m_client;
    QString m_cacheDir;
    // Newest requests first: the covers on screen now, not ones scrolled past
    QList<Request> m_queue;
    QSet<QString> m_wanted;
    int m_running = 0;
};
