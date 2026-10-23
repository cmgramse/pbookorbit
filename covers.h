#pragma once

#include <QList>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QString>

class BookOrbitClient;

// Cover thumbnails for the book list. Downloads each book's thumbnail from BookOrbit
// (GET /books/:id/thumbnail, about 100 KB), shrinks it to the size it is shown at and
// keeps it as a small JPEG on the device, so it loads straight from storage next time.
//
// QML asks for a cover with source(); if it isn't cached yet the result is empty, a
// download starts, and ready(bookId) follows once source() has it.
class Covers : public QObject
{
    Q_OBJECT

public:
    Covers(BookOrbitClient *client, const QString &cacheDir, QObject *parent = nullptr);

    // File URL of the cached cover, or "" while it downloads. version is the book's
    // coverVersion, so a changed cover is fetched again.
    Q_INVOKABLE QString source(int bookId, const QString &version, int width, int height);

signals:
    void ready(int bookId);

private:
    struct Request {
        int bookId;
        QString version;
        QSize size;
        QString file;
    };

    QString cacheFile(int bookId, const QString &version, const QSize &size) const;
    void startDownloads();

    BookOrbitClient *m_client;
    QString m_cacheDir;
    // Newest requests first: the covers on screen now, not ones scrolled past
    QList<Request> m_queue;
    QSet<QString> m_wanted;
    int m_running = 0;
};
