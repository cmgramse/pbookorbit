#pragma once

#include "downloads.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QVariantList>
#include <QVariantMap>

class BookOrbitClient;

// Finds books that were on the device before the app, such as ones downloaded through
// KOReader, and records the ones in the BookOrbit library as downloaded.
//
// Looks through the Books, Audio Books and Downloads folders for book files the app
// hasn't recorded. A file is the library's book when, in order:
//   1. KOReader's BookOrbit plugin says so: its state file (bookorbit_sync_state*.lua)
//      lists the book and file ids of every file it downloaded or matched. Only for this
//      server, and only if the file is still the one it saw (same partial MD5).
//   2. Its partial MD5, the hash KOReader identifies books by, is a library file's.
//   3. Its name is a library file's name.
// For 2 and 3 the candidates are the library books whose title is in the file's name, as
// whole words, with the same format; the best three are looked up. A file whose name has
// a book's title and author but matched neither way is only a guess: those are kept as
// unsure, for the user to add or not.
class BookFinder : public QObject
{
    Q_OBJECT
    // "idle", "searching", "done" or "failed"
    Q_PROPERTY(QString state READ state NOTIFY changed)
    // How far the search is, 0-100
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    // Once done: the books found and recorded, the guesses (each {fileName, title}), and
    // how many files are in neither
    Q_PROPERTY(int foundCount READ foundCount NOTIFY changed)
    Q_PROPERTY(QVariantList unsure READ unsure NOTIFY changed)
    Q_PROPERTY(int notFoundCount READ notFoundCount NOTIFY changed)
    // Why the search failed
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    BookFinder(BookOrbitClient *client, Downloads *downloads, const QString &settingsPath,
               QObject *parent = nullptr);

    QString state() const { return m_state; }
    int percent() const { return m_percent; }
    int foundCount() const { return m_foundCount; }
    QVariantList unsure() const;
    int notFoundCount() const { return m_notFoundCount; }
    QString error() const { return m_error; }

    // Book files on the device that the app hasn't recorded
    Q_INVOKABLE int unrecordedCount() const;
    // Whether to offer a search: once, after the first sign-in, if there's anything to find
    Q_INVOKABLE bool shouldOffer() const;
    Q_INVOKABLE void markOffered();
    Q_INVOKABLE void start();
    Q_INVOKABLE void cancel();
    // Records the unsure books too
    Q_INVOKABLE void addUnsure();

signals:
    void changed();

private:
    struct LocalFile {
        QString path;
        QString fileName;
        QString format;     // the suffix, lower case
        QString nameKey;    // the name without its suffix, normalised (see normalise())
        QString md5;        // KOReader's partial MD5
        QList<int> candidates;   // indexes into m_library, best first
    };
    struct KoreaderMatch {
        int bookId;
        QString md5;
    };

    QList<LocalFile> unrecordedFiles() const;
    // From the KOReader plugin's state files for this server: local path -> its match
    QHash<QString, KoreaderMatch> koreaderMatches() const;
    void loadLibraryPage(int libraryId, int page, int generation);
    // With the whole library in, sorts the files into found and to look up
    void matchFiles(int generation);
    // Looks up the next candidate of the file at m_pending[index]
    void checkCandidate(int index, int candidate, int generation);
    void finish(int generation);
    void fail(const QString &error);
    void setPercent(int percent);

    BookOrbitClient *m_client;
    Downloads *m_downloads;
    QSettings m_settings;
    QString m_state = "idle";
    int m_percent = 0;
    int m_foundCount = 0;
    int m_notFoundCount = 0;
    QString m_error;
    // Bumped by each search, so replies to a cancelled one are ignored
    int m_generation = 0;

    // The search under way
    QList<LocalFile> m_files;
    QHash<QString, KoreaderMatch> m_koreader;
    QList<QVariantMap> m_library;   // list entries
    QList<QString> m_titleKeys;     // normalised titles, as m_library
    QList<QString> m_authorKeys;    // normalised last name of the first author
    QHash<int, int> m_libraryIndex; // book id -> index in m_library
    int m_libraryTotal = 0;
    QList<LocalFile> m_pending;     // files to look up on the server
    QList<Downloads::Existing> m_found;
    QList<Downloads::Existing> m_unsure;
};
