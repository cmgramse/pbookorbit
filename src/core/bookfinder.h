#pragma once

#include "core/downloads.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class BookOrbitClient;
class Database;

// Finds books that were already on the device (e.g. downloaded through KOReader) and
// records the ones in the BookOrbit library as downloaded.
//
// Scans Books, Audio Books and Downloads for files the app hasn't recorded. A file matches
// a library book if, in order:
//   1. KOReader's BookOrbit plugin lists it in its state file (bookorbit_sync_state*.lua)
//      for this server, and the file is unchanged (same partial MD5).
//   2. Its partial MD5, KOReader's book hash, matches a library file.
//   3. Its name matches a library file's name.
// Candidates for 2 and 3 are library books in the same format whose title appears in the
// file name as whole words, those with the author in the name too first; the best three are
// looked up. A file that matches none of them is kept as an unsure guess, the best, which
// isn't recorded: they're for the user to confirm, once there's a list to do that in.
class BookFinder : public QObject
{
    Q_OBJECT
    // "idle", "searching", "done" or "failed"
    Q_PROPERTY(QString state READ state NOTIFY changed)
    // How far the search is, 0-100
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    // When done: books found and recorded, guesses ({fileName, title} each, not recorded),
    // and how many files matched neither
    Q_PROPERTY(int foundCount READ foundCount NOTIFY changed)
    Q_PROPERTY(QVariantList unsure READ unsure NOTIFY changed)
    Q_PROPERTY(int notFoundCount READ notFoundCount NOTIFY changed)
    // Why the search failed
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    BookFinder(BookOrbitClient *client, Downloads *downloads, Database *database,
               QObject *parent = nullptr);

    QString state() const { return m_state; }
    int percent() const { return m_percent; }
    int foundCount() const { return m_foundCount; }
    QVariantList unsure() const;
    int notFoundCount() const { return m_notFoundCount; }
    QString error() const { return m_error; }

    // Book files on the device the app hasn't recorded
    Q_INVOKABLE int unrecordedCount() const;
    // Whether to offer a search: once, after the first sign-in, if there's anything to find
    Q_INVOKABLE bool shouldOffer() const;
    Q_INVOKABLE void markOffered();
    Q_INVOKABLE void start();
    Q_INVOKABLE void cancel();

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
    // Matches from the KOReader plugin's state files for this server: local path -> match
    QHash<QString, KoreaderMatch> koreaderMatches() const;
    void loadLibraryPage(int libraryId, int page, int generation);
    // Once the library has loaded, splits files into found and still to look up
    void matchFiles(int generation);
    // Looks up the next candidate for m_pending[index]
    void checkCandidate(int index, int candidate, int generation);
    void finish(int generation);
    void fail(const QString &error);
    void setPercent(int percent);

    BookOrbitClient *m_client;
    Downloads *m_downloads;
    Database *m_database;
    QString m_state = "idle";
    int m_percent = 0;
    int m_foundCount = 0;
    int m_notFoundCount = 0;
    QString m_error;
    // Bumped per search, so replies to a cancelled one are ignored
    int m_generation = 0;

    // The current search
    QList<LocalFile> m_files;
    QHash<QString, KoreaderMatch> m_koreader;
    QList<QVariantMap> m_library;   // list entries
    QList<QString> m_titleKeys;     // normalised titles, parallel to m_library
    QList<QString> m_authorKeys;    // normalised last name of the first author
    QHash<int, int> m_libraryIndex; // book id -> index in m_library
    int m_libraryTotal = 0;
    QList<LocalFile> m_pending;     // files to look up on the server
    QList<Downloads::Existing> m_found;
    QList<Downloads::Existing> m_unsure;
};
