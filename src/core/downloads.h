#pragma once

#include <QFile>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

class BookOrbitClient;
class Database;
class QNetworkReply;

// Downloads books to the device and opens them in the system reader.
//
// Ebooks go to /mnt/ext1/Books and audiobooks to Audio Books, under the server's file name.
// Where a book has several formats, the primary file is used. Each download's path and list
// entry are saved in the database, so a book counts as downloaded while its file exists,
// and downloaded books can be listed without the server.
//
// One download at a time. It streams to a hidden ".part" file that's renamed when complete,
// so the system library never sees a partial book.
class Downloads : public QObject
{
    Q_OBJECT
    // The book downloading now, or -1
    Q_PROPERTY(int bookId READ bookId NOTIFY changed)
    // Progress, 0-100, in whole percent so the screen redraws rarely
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    // Why the last download failed, or ""
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // Bumped when a book arrives, so QML re-checks isOnDevice()
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    Downloads(BookOrbitClient *client, Database *database, QObject *parent = nullptr);

    int bookId() const { return m_bookId; }
    int percent() const { return m_percent; }
    QString error() const { return m_error; }
    int revision() const { return m_revision; }

    Q_INVOKABLE bool isOnDevice(int bookId) const;
    // Formats of a book's files on the device, e.g. ["epub"]
    Q_INVOKABLE QStringList formatsOnDevice(int bookId) const;
    // Downloaded books' list entries as saved at download time, plus downloadedFormats
    Q_INVOKABLE QVariantList downloadedBooks() const;
    // The file a download would fetch, {id, format, sizeBytes, folder} where folder is
    // "Books" or "Audio Books". The primary file, or fileId's. Empty if none.
    Q_INVOKABLE QVariantMap fileToDownload(const QVariantMap &book, int fileId = -1) const;
    // Downloads the primary file, or fileId's. Emits downloaded(bookId), or sets error.
    Q_INVOKABLE void download(const QVariantMap &book, int fileId = -1);
    Q_INVOKABLE void cancel();
    // Opens a downloaded book: ebooks in the reader, audiobooks in the audiobook app
    Q_INVOKABLE bool open(int bookId);
    // Apps that can open this format, as the system's Open with menu lists them, e.g.
    // [{app: "koreader.app", name: "KOReader"}]. Read from the firmware's and
    // /mnt/ext1/system/config's extensions.cfg (KOReader registers itself in the latter).
    Q_INVOKABLE QVariantList appsFor(const QString &format) const;
    // Opens the book's file in app (from appsFor()) and saves that choice in handlers.cfg,
    // like the system's Open with, so the system library uses it too
    Q_INVOKABLE bool openWith(int bookId, const QString &format, const QString &app);
    // The book's files on the device
    Q_INVOKABLE QStringList pathsOnDevice(int bookId) const;
    // Deletes the book's files and forgets them. Returns false if a file couldn't be
    // deleted (it stays recorded).
    Q_INVOKABLE bool remove(int bookId);

    // Every recorded book file, whether or not it still exists
    QSet<QString> recordedPaths() const;
    // A book file found on the device, and the book it matched
    struct Existing {
        int bookId;
        QString path;
        QVariantMap entry;   // the book's list entry
    };
    // Records them as though the app had downloaded them
    void addExisting(const QList<Existing> &books);
    // Bumps revision, for saved entries updated elsewhere
    void entriesChanged();

signals:
    void changed();
    void downloaded(int bookId);

private:
    // Asks the system to scan for new books, so downloads show in its library
    void requestLibraryScan();
    // Ends the download, deleting the part file unless it was kept, and sets error
    void finish(const QString &error);
    void setPercent(int percent);

    BookOrbitClient *m_client;
    Database *m_database;
    int m_bookId = -1;
    int m_percent = 0;
    QString m_error;
    int m_revision = 0;
    // The current download
    QPointer<QNetworkReply> m_reply;
    QFile m_part;
    QString m_fileName;      // from the server's Content-Disposition
    QString m_fallbackName;  // "<title>.<format>" if it doesn't give one
    QVariantMap m_entry;     // the list entry of the book downloading, to save
    // Bumped per download, so callbacks for a cancelled one are ignored
    int m_generation = 0;
};
