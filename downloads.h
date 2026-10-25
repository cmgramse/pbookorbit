#pragma once

#include <QFile>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QVariantMap>

class BookOrbitClient;
class QNetworkReply;

// Books downloaded to the device, and opening them in the system reader.
//
// A book downloads to the device's Books folder (/mnt/ext1/Books), or Audio Books for an
// audiobook, under the file name the server gives it. Its primary file is the one taken
// when it has several formats. The app remembers where each book went, in its settings
// file, so a book counts as on the device while that file is still there. It keeps each
// book's list entry too, so the downloaded books can be listed without the server.
//
// One download at a time. It streams to a hidden ".part" file in the folder it is going
// to, which is renamed once complete, so the system library never sees half a book.
class Downloads : public QObject
{
    Q_OBJECT
    // The book downloading now, or -1
    Q_PROPERTY(int bookId READ bookId NOTIFY changed)
    // How much of it has arrived, 0-100, in whole percent so the screen redraws rarely
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    // Why the last download failed, or ""
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // Bumped whenever a book arrives on the device, so QML can check isOnDevice() again
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    Downloads(BookOrbitClient *client, const QString &settingsPath, QObject *parent = nullptr);

    int bookId() const { return m_bookId; }
    int percent() const { return m_percent; }
    QString error() const { return m_error; }
    int revision() const { return m_revision; }

    Q_INVOKABLE bool isOnDevice(int bookId) const;
    // The formats of a book's files on the device, e.g. ["epub"]
    Q_INVOKABLE QStringList formatsOnDevice(int bookId) const;
    // The books on the device, as their list entries were when they downloaded, plus
    // downloadedFormats: the formats of their files on the device
    Q_INVOKABLE QVariantList downloadedBooks() const;
    // The file a download would fetch: {id, format, sizeBytes, folder}, folder being
    // "Books" or "Audio Books"; empty if there's none. The primary file, or fileId's.
    Q_INVOKABLE QVariantMap fileToDownload(const QVariantMap &book, int fileId = -1) const;
    // Downloads the book's primary file, or fileId's; downloaded(bookId) follows, or
    // error is set
    Q_INVOKABLE void download(const QVariantMap &book, int fileId = -1);
    Q_INVOKABLE void cancel();
    // Opens a downloaded book in the system reader
    Q_INVOKABLE bool open(int bookId);

    // Every book file the app has recorded, whether or not it is still there
    QSet<QString> recordedPaths() const;
    // A book file that was on the device already, and the book it is
    struct Existing {
        int bookId;
        QString path;
        QVariantMap entry;   // the book's list entry
    };
    // Records them as if the app had downloaded them
    void addExisting(const QList<Existing> &books);

signals:
    void changed();
    void downloaded(int bookId);

private:
    // Where a book's files went: one, or two for a record with an ebook and audiobook
    QStringList pathsFor(int bookId) const;
    // Asks the system to scan for new books, so a download shows in its library
    void requestLibraryScan();
    // Ends the download: deletes the part file unless it was kept, and sets error
    void finish(const QString &error);
    void setPercent(int percent);

    BookOrbitClient *m_client;
    // mutable: reading a group's keys means entering it
    mutable QSettings m_settings;
    int m_bookId = -1;
    int m_percent = 0;
    QString m_error;
    int m_revision = 0;
    // The download under way
    QPointer<QNetworkReply> m_reply;
    QFile m_part;
    QString m_fileName;      // from the server's Content-Disposition
    QString m_fallbackName;  // "<title>.<format>" if it doesn't give one
    QVariantMap m_entry;     // the book downloading, as the list has it, for the records
    // Bumped by each download, so callbacks for a cancelled one are ignored
    int m_generation = 0;
};
