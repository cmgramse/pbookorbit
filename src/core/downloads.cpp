#include "core/downloads.h"

#include "core/bookorbitclient.h"
#include "core/database.h"
#include "core/diagnostics.h"
#include "core/formats.h"

#include <common_utilities/einkinterface.h>
#include <inkview.h>
#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>

// Download folders: ebooks to Books, audiobooks to Audio Books (where the audiobook player
// looks). /mnt/ext1/... on the device, ./system/mnt/ext1/... in the emulator.
static const char *BOOKS_DIR = FLASHDIR "/Books";
static const char *AUDIOBOOKS_DIR = AUDIOBOOKS_DEFAULT_PATH;
// The system audiobook player (Audiobooks on the home screen)
static const char *AUDIOBOOKS_APP = "/ebrmain/bin/audio_books.app";

// Audiobook formats, as in BookOrbit's AUDIO_FORMAT_LIST

// Folder for a file of this format
static QString folderFor(const QString &format)
{
    return QString::fromUtf8(isAudioFormat(format) ? AUDIOBOOKS_DIR : BOOKS_DIR);
}

// A file name the device accepts: no path separators or other awkward characters
static QString safeFileName(QString name)
{
    name.replace(QRegularExpression(R"([/\\:*?"<>|\x00-\x1f])"), "_");
    name = name.trimmed();
    while (name.startsWith('.'))
        name.remove(0, 1);   // not hidden
    return name.left(200);
}

// The file name from a Content-Disposition header, preferring the UTF-8 filename*, e.g.
// attachment; filename="Dune (2007).m4b"; filename*=UTF-8''Dune%20%282007%29.m4b
static QString dispositionFileName(const QByteArray &header)
{
    static const QRegularExpression extended(R"(filename\*\s*=\s*UTF-8''([^;\s]+))",
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression plain(R"(filename\s*=\s*"([^"]*)\")",
                                          QRegularExpression::CaseInsensitiveOption);
    const QString text = QString::fromUtf8(header);
    QRegularExpressionMatch match = extended.match(text);
    if (match.hasMatch())
        return QUrl::fromPercentEncoding(match.captured(1).toLatin1());
    match = plain.match(text);
    return match.hasMatch() ? match.captured(1) : QString();
}

Downloads::Downloads(BookOrbitClient *client, Database *database, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_database(database)
{
    // Logs the scanner's replies to requestLibraryScan()
    if (auto *platform = pocketbook::utilities::einkPlatform()) {
        platform->addHandlerForInkvewEvents([](int type, int, int) {
            if (type == EVT_SCANSTOPPED)
                diagnostics::trace("library scan finished");
        });
    }
}

void Downloads::entriesChanged()
{
    m_revision++;
    emit changed();
}

QVariantList Downloads::downloadedBooks() const
{
    QVariantList books;
    for (const Database::Book &record : m_database->booksWithFiles()) {
        // Formats on the device, e.g. ["epub"], for the content type filter
        QStringList formats;
        for (const QString &path : record.files) {
            if (QFileInfo::exists(path))
                formats.append(QFileInfo(path).suffix().toLower());
        }
        if (formats.isEmpty())
            continue;
        QVariantMap book = record.entry;
        book.insert("downloadedFormats", formats);
        books.append(book);
    }
    return books;
}

QStringList Downloads::formatsOnDevice(int bookId) const
{
    QStringList formats;
    for (const QString &path : m_database->bookFiles(bookId)) {
        if (QFileInfo::exists(path))
            formats.append(QFileInfo(path).suffix().toLower());
    }
    return formats;
}

QStringList Downloads::kindsOnDevice(int bookId) const
{
    QStringList kinds;
    for (const QString &format : formatsOnDevice(bookId)) {
        if (!kinds.contains(mediaKind(format)))
            kinds.append(mediaKind(format));
    }
    return kinds;
}

QString Downloads::kindOf(const QString &format) const
{
    return mediaKind(format);
}

QStringList Downloads::pathsOnDevice(int bookId) const
{
    QStringList paths;
    for (const QString &path : m_database->bookFiles(bookId)) {
        if (QFileInfo::exists(path))
            paths.append(path);
    }
    return paths;
}

bool Downloads::remove(int bookId, const QString &kind)
{
    // Missing files count as deleted. Notes and positions beside the book (KOReader's .sdr
    // folder) are left, as is the database entry.
    bool deleted = true;
    m_database->transaction([&] {
        for (const QString &path : m_database->bookFiles(bookId)) {
            if (!kind.isEmpty() && mediaKind(QFileInfo(path).suffix()) != kind)
                continue;
            if (QFileInfo::exists(path) && !QFile::remove(path)) {
                diagnostics::log(QString("book %1: can't delete %2").arg(bookId).arg(path));
                deleted = false;
            } else {
                diagnostics::trace(QString("book %1 deleted from %2").arg(bookId).arg(path));
                m_database->removeBookFile(path);
            }
        }
    });
    // So the system library drops it too
    requestLibraryScan();
    m_revision++;
    emit changed();
    return deleted;
}

bool Downloads::isOnDevice(int bookId) const
{
    for (const QString &path : m_database->bookFiles(bookId)) {
        if (QFileInfo::exists(path))
            return true;
    }
    return false;
}

QVariantMap Downloads::fileToDownload(const QVariantMap &book, int fileId) const
{
    // files: [{id, format, role, sizeBytes}]. "primary" is the book in its main format and
    // "content" the same book in another. "cover", "metadata" and "supplement" files aren't
    // the book.
    QVariantMap chosen;
    for (const QVariant &value : book.value("files").toList()) {
        const QVariantMap file = value.toMap();
        const QString role = file.value("role").toString();
        if (file.value("format").toString().isEmpty() || (role != "primary" && role != "content"))
            continue;
        if (fileId >= 0) {
            if (file.value("id").toInt() == fileId) {
                chosen = file;
                break;
            }
            continue;
        }
        if (role == "primary") {
            chosen = file;
            break;
        }
        if (chosen.isEmpty())
            chosen = file;
    }
    // Destination: "Books" or "Audio Books"
    if (!chosen.isEmpty())
        chosen.insert("folder", QFileInfo(folderFor(chosen.value("format").toString())).fileName());
    return chosen;
}

void Downloads::download(const QVariantMap &book, int fileId)
{
    if (m_bookId != -1)
        return;   // one at a time
    const int bookId = book.value("id").toInt();
    const QVariantMap file = fileToDownload(book, fileId);
    m_generation++;
    m_bookId = bookId;
    m_percent = 0;
    m_error.clear();
    // finish() deletes whatever this names, so never point it at a finished book
    m_part.setFileName(QString());
    emit changed();
    if (file.isEmpty()) {
        finish("This book has no file to download");
        return;
    }

    const QString format = file.value("format").toString();
    m_fileName.clear();
    m_fallbackName = safeFileName(book.value("title").toString()) + "." + format;
    // The server's list entry, without the fields the list adds
    m_entry = book;
    for (const char *added : {"loaded", "section", "sectionLabel", "sectionStart", "statusText", "seriesText"})
        m_entry.remove(added);
    const QString folder = folderFor(format);
    const QString folderName = file.value("folder").toString();
    if (!QDir().mkpath(folder)) {
        finish(QString("Can't create the %1 folder").arg(folderName));
        return;
    }
    m_part.setFileName(QString("%1/.pbookorbit-%2.part").arg(folder).arg(file.value("id").toInt()));
    if (!m_part.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        finish(QString("Can't write to the %1 folder: %2").arg(folderName, m_part.errorString()));
        return;
    }

    const int generation = m_generation;
    m_client->startStream(QString("/books/files/%1/download").arg(file.value("id").toInt()),
        [this, generation, bookId, folder](QNetworkReply *reply) {
            if (generation != m_generation) {
                // Cancelled while signing in
                reply->abort();
                reply->deleteLater();
                return;
            }
            m_reply = reply;
            connect(reply, &QNetworkReply::metaDataChanged, this, [this, reply]() {
                const QString name = dispositionFileName(reply->rawHeader("Content-Disposition"));
                if (!name.isEmpty())
                    m_fileName = safeFileName(name);
            });
            connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (status != 200 && status != 206)
                    return;   // an error message, not the book
                const QByteArray data = reply->readAll();
                if (m_part.write(data) != data.size()) {
                    m_error = "Can't write the book: " + m_part.errorString();
                    reply->abort();
                }
            });
            connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
                if (total > 0)
                    setPercent(int(received * 100 / total));
            });
            connect(reply, &QNetworkReply::finished, this, [this, reply, generation, bookId, folder]() {
                reply->deleteLater();
                if (generation != m_generation)
                    return;   // cancelled
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                diagnostics::trace(QString("book %1 download -> %2, %3 bytes")
                                     .arg(bookId)
                                     .arg(status ? QString::number(status) : reply->errorString())
                                     .arg(m_part.size()));
                if (reply->error() != QNetworkReply::NoError) {
                    finish(!m_error.isEmpty() ? m_error
                           : status ? QString("The server couldn't send the book (HTTP %1)").arg(status)
                                    : reply->errorString());
                    return;
                }
                m_part.close();

                // Keep the server's name, without overwriting a different book with the
                // same name
                const QString name = m_fileName.isEmpty() ? m_fallbackName : m_fileName;
                const QFileInfo info(name);
                QString path = QString("%1/%2").arg(folder, name);
                for (int n = 2; QFileInfo::exists(path); n++)
                    path = QString("%1/%2 (%3).%4").arg(folder, info.completeBaseName()).arg(n).arg(info.suffix());
                if (!m_part.rename(path)) {
                    finish("Can't save the book: " + m_part.errorString());
                    return;
                }
                m_part.setFileName(QString());   // the book now, not the part file
                m_database->transaction([&] {
                    m_database->saveBook(bookId, m_entry);
                    m_database->addBookFile(bookId, path);
                });
                diagnostics::trace(QString("book %1 saved as %2").arg(bookId).arg(path));
                requestLibraryScan();
                m_reply = nullptr;
                m_bookId = -1;
                m_percent = 0;
                m_revision++;
                emit changed();
                emit downloaded(bookId);
            });
        },
        [this, generation](int, const QString &message) {
            if (generation == m_generation)
                finish(message);
        });
}

void Downloads::requestLibraryScan()
{
    // Undocumented: the SDK has no call to add a book to the library. This broadcasts
    // EVT_STARTSCAN to other tasks (OTHERTASKS, -3) so the resident scanner service
    // rescans, as pbweb does and the firmware does in PBScanClient::startDeviceScan. It
    // replies with EVT_SCANSTOPPED. See NOTES.md.
    const int rc = SendEventTo(OTHERTASKS, EVT_STARTSCAN, 0, 0);
    (rc < 0 ? diagnostics::log : diagnostics::trace)(QString("library scan requested -> %1").arg(rc));
}

void Downloads::cancel()
{
    if (m_bookId == -1)
        return;
    m_generation++;   // the reply's handlers ignore it from now on
    if (m_reply)
        m_reply->abort();
    finish(QString());
}

void Downloads::finish(const QString &error)
{
    // The part file, if any (download() and a successful save clear its name)
    if (m_part.isOpen())
        m_part.close();
    if (!m_part.fileName().isEmpty() && m_part.exists())
        m_part.remove();
    m_part.setFileName(QString());
    m_reply = nullptr;
    m_bookId = -1;
    m_percent = 0;
    m_error = error;
    if (!error.isEmpty())
        diagnostics::log("download failed: " + error);
    emit changed();
}

void Downloads::setPercent(int percent)
{
    if (percent == m_percent)
        return;
    m_percent = percent;
    emit changed();
}

QSet<QString> Downloads::recordedPaths() const
{
    return m_database->allBookFiles();
}

void Downloads::addExisting(const QList<Existing> &books)
{
    if (books.isEmpty())
        return;
    m_database->transaction([&] {
        for (const Existing &book : books) {
            m_database->saveBook(book.bookId, book.entry);
            m_database->addBookFile(book.bookId, book.path);
            diagnostics::trace(QString("book %1 found at %2").arg(book.bookId).arg(book.path));
        }
    });
    m_revision++;
    emit changed();
}

QVariantList Downloads::appsFor(const QString &format) const
{
    // One entry per extensions.cfg line for the format. The emulator reads its file twice,
    // so apps can repeat.
    QStringList apps;
    const iv_filetype *types = GetSupportedFileTypes();
    const int count = GetSupportedFileTypesLength();
    for (int i = 0; types && i < count; i++) {
        if (!types[i].extension || QString::fromUtf8(types[i].extension).compare(format, Qt::CaseInsensitive) != 0)
            continue;
        // "koreader.app,eink-reader_with_blink.app,eink-reader_with_epub2.app"
        for (const QString &app : QString::fromUtf8(types[i].program ? types[i].program : "").split(',', Qt::SkipEmptyParts)) {
            if (!apps.contains(app.trimmed()))
                apps.append(app.trimmed());
        }
    }

    QVariantList result;
    for (const QString &app : apps) {
        // Names come from language strings named after the app: "@eink-reader_with_blink"
        // is "PB Reader (EPUB 3)". Other apps have none unless the user adds one (e.g.
        // "@koreader=KOReader") to the language file on their storage.
        QString base = app;
        if (base.endsWith(".app"))
            base.chop(4);
        const QByteArray key = ("@" + base).toUtf8();
        const char *text = GetLangText(key.constData());
        const QString name = text && key != text ? QString::fromUtf8(text) : base;
        result.append(QVariantMap{{"app", app}, {"name", name}});
    }
    return result;
}

bool Downloads::openWith(int bookId, const QString &format, const QString &app)
{
    QString path;
    for (const QString &candidate : m_database->bookFiles(bookId)) {
        if (QFileInfo::exists(candidate) && QFileInfo(candidate).suffix().compare(format, Qt::CaseInsensitive) == 0)
            path = candidate;
    }
    if (path.isEmpty())
        return false;
    const QByteArray file = path.toUtf8();
    SetFileHandler(file.constData(), app.toUtf8().constData());
    const int rc = OpenBook(file.constData(), "", OB_ADDTOLAST | OB_WITHRETURN);
    (rc < 0 ? diagnostics::log : diagnostics::trace)(QString("OpenBook %1 with %2 -> %3").arg(path, app).arg(rc));
    return true;
}

bool Downloads::open(int bookId, const QString &kind)
{
    // The ebook, if both are present
    QString path;
    for (const QString &candidate : m_database->bookFiles(bookId)) {
        if (!kind.isEmpty() && mediaKind(QFileInfo(candidate).suffix()) != kind)
            continue;
        if (QFileInfo::exists(candidate) && (path.isEmpty() || isAudioFormat(QFileInfo(path).suffix())))
            path = candidate;
    }
    if (path.isEmpty())
        return false;
    if (isAudioFormat(QFileInfo(path).suffix())) {
        // OpenBook() uses extensions.cfg, which lists no audiobook formats (only mp3, for
        // the music player), so it fails with "Unknown book type". The audiobook app takes
        // --bookPlay <path> (see its parseArguments()). OpenTask starts it, or passes the
        // arguments to a running instance and brings it forward.
        const QByteArray file = path.toUtf8();
        const char *args[] = {"--bookPlay", file.constData()};
        const int rc = OpenTask(AUDIOBOOKS_APP, 2, args, TASK_MAKEACTIVE);
        (rc < 0 ? diagnostics::log : diagnostics::trace)(QString("OpenTask %1 --bookPlay %2 -> %3").arg(AUDIOBOOKS_APP, path).arg(rc));
        return rc >= 0;
    }
    // Adds it to the system's recent books, and returns here when the reader closes
    const int rc = OpenBook(path.toUtf8().constData(), "", OB_ADDTOLAST | OB_WITHRETURN);
    (rc < 0 ? diagnostics::log : diagnostics::trace)(QString("OpenBook %1 -> %2").arg(path).arg(rc));
    return true;
}
