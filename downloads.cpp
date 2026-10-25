#include "downloads.h"

#include "bookorbitclient.h"
#include "diagnostics.h"
#include "formats.h"

#include <common_utilities/einkinterface.h>
#include <inkview.h>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>

// Where downloads go on the main storage: ebooks to Books, audiobooks to Audio Books,
// where the system's audiobook player looks (/mnt/ext1/... on the device,
// ./system/mnt/ext1/... in the emulator)
static const char *BOOKS_DIR = FLASHDIR "/Books";
static const char *AUDIOBOOKS_DIR = AUDIOBOOKS_DEFAULT_PATH;
// The system's audiobook player (Audiobooks on the home screen)
static const char *AUDIOBOOKS_APP = "/ebrmain/bin/audio_books.app";

// Audiobook formats, as BookOrbit lists them (AUDIO_FORMAT_LIST)

// The folder a file of this format goes to
static QString folderFor(const QString &format)
{
    return QString::fromUtf8(isAudioFormat(format) ? AUDIOBOOKS_DIR : BOOKS_DIR);
}

// A name the device's file system takes: no path separators or other awkward characters
static QString safeFileName(QString name)
{
    name.replace(QRegularExpression(R"([/\\:*?"<>|\x00-\x1f])"), "_");
    name = name.trimmed();
    while (name.startsWith('.'))
        name.remove(0, 1);   // not hidden
    return name.left(200);
}

// The file name in a Content-Disposition header: the UTF-8 filename* if there is one,
// e.g. attachment; filename="Dune (2007).m4b"; filename*=UTF-8''Dune%20%282007%29.m4b
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

Downloads::Downloads(BookOrbitClient *client, const QString &settingsPath, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_settings(settingsPath, QSettings::IniFormat)
{
    // The system scanner's replies to requestLibraryScan(), logged to see it working
    if (auto *platform = pocketbook::utilities::einkPlatform()) {
        platform->addHandlerForInkvewEvents([](int type, int, int) {
            if (type == EVT_SCANSTOPPED)
                diagnostics::log("library scan finished");
        });
    }

    // Earlier versions kept only each book's title and authors; its entry replaces them
    m_settings.remove("downloadTitles");
    m_settings.remove("downloadAuthors");
}

QVariantList Downloads::downloadedBooks() const
{
    QVariantList books;
    m_settings.beginGroup("downloads");
    const QStringList ids = m_settings.childKeys();
    m_settings.endGroup();
    for (const QString &id : ids) {
        const QByteArray entry = m_settings.value("downloadEntries/" + id).toByteArray();
        if (entry.isEmpty() || !isOnDevice(id.toInt()))
            continue;
        QVariantMap book = QJsonDocument::fromJson(entry).object().toVariantMap();
        // The formats on the device, e.g. ["epub"], for the content type filter
        book.insert("downloadedFormats", formatsOnDevice(id.toInt()));
        books.append(book);
    }
    return books;
}

QStringList Downloads::pathsFor(int bookId) const
{
    // A list since a record can hold an ebook and an audiobook; earlier versions kept a
    // single path, which reads back as a list of one
    return m_settings.value(QString("downloads/%1").arg(bookId)).toStringList();
}

QStringList Downloads::formatsOnDevice(int bookId) const
{
    QStringList formats;
    for (const QString &path : pathsFor(bookId)) {
        if (QFileInfo::exists(path))
            formats.append(QFileInfo(path).suffix().toLower());
    }
    return formats;
}

bool Downloads::isOnDevice(int bookId) const
{
    for (const QString &path : pathsFor(bookId)) {
        if (QFileInfo::exists(path))
            return true;
    }
    return false;
}

QVariantMap Downloads::fileToDownload(const QVariantMap &book, int fileId) const
{
    // files: [{id, format, role, sizeBytes}]. The role says what the file is: "primary"
    // is the book in the format the server holds it in first, "content" the same book in
    // another format; "cover", "metadata" and "supplement" files aren't the book at all.
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
    // Where it will go: "Books" or "Audio Books"
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
    // finish() deletes whatever this names, so never a finished book
    m_part.setFileName(QString());
    emit changed();
    if (file.isEmpty()) {
        finish("This book has no file to download");
        return;
    }

    const QString format = file.value("format").toString();
    m_fileName.clear();
    m_fallbackName = safeFileName(book.value("title").toString()) + "." + format;
    // The book's list entry as the server sent it, without the fields the list adds
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
                diagnostics::log(QString("book %1 download -> %2, %3 bytes")
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

                // Under the server's name, without replacing a different book of the same name
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
                QStringList paths = pathsFor(bookId);
                paths.append(path);
                m_settings.setValue(QString("downloads/%1").arg(bookId), paths);
                m_settings.setValue(QString("downloadEntries/%1").arg(bookId),
                                    QJsonDocument(QJsonObject::fromVariantMap(m_entry)).toJson(QJsonDocument::Compact));
                m_settings.sync();
                diagnostics::log(QString("book %1 saved as %2").arg(bookId).arg(path));
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
    // EVT_STARTSCAN to the other tasks (OTHERTASKS, -3), asking the system's resident
    // scanner service to scan for new books, as pbweb does and says the firmware does
    // (PBScanClient::startDeviceScan). It answers with EVT_SCANSTOPPED. See NOTES.md.
    const int rc = SendEventTo(OTHERTASKS, EVT_STARTSCAN, 0, 0);
    diagnostics::log(QString("library scan requested -> %1").arg(rc));
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
    // The part file, if there is one (download() and a successful save clear its name)
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
    QSet<QString> paths;
    m_settings.beginGroup("downloads");
    const QStringList ids = m_settings.childKeys();
    m_settings.endGroup();
    for (const QString &id : ids) {
        for (const QString &path : pathsFor(id.toInt()))
            paths.insert(path);
    }
    return paths;
}

void Downloads::addExisting(const QList<Existing> &books)
{
    if (books.isEmpty())
        return;
    for (const Existing &book : books) {
        QStringList paths = pathsFor(book.bookId);
        if (!paths.contains(book.path))
            paths.append(book.path);
        m_settings.setValue(QString("downloads/%1").arg(book.bookId), paths);
        m_settings.setValue(QString("downloadEntries/%1").arg(book.bookId),
                            QJsonDocument(QJsonObject::fromVariantMap(book.entry)).toJson(QJsonDocument::Compact));
        diagnostics::log(QString("book %1 found at %2").arg(book.bookId).arg(book.path));
    }
    m_settings.sync();
    m_revision++;
    emit changed();
}

bool Downloads::open(int bookId)
{
    // The ebook if both are here
    QString path;
    for (const QString &candidate : pathsFor(bookId)) {
        if (QFileInfo::exists(candidate) && (path.isEmpty() || isAudioFormat(QFileInfo(path).suffix())))
            path = candidate;
    }
    if (path.isEmpty())
        return false;
    if (isAudioFormat(QFileInfo(path).suffix())) {
        // OpenBook() goes by extensions.cfg, which has no audiobook formats (only mp3, for
        // the music player), so it says "Unknown book type". The system's audiobook app
        // plays a book given --bookPlay <path>, read from its parseArguments(); OpenTask
        // starts it, or hands a running one the arguments and brings it to the front.
        const QByteArray file = path.toUtf8();
        const char *args[] = {"--bookPlay", file.constData()};
        const int rc = OpenTask(AUDIOBOOKS_APP, 2, args, TASK_MAKEACTIVE);
        diagnostics::log(QString("OpenTask %1 --bookPlay %2 -> %3").arg(AUDIOBOOKS_APP, path).arg(rc));
        return rc >= 0;
    }
    // Adds it to the system's recent books, and comes back here when the reader closes
    const int rc = OpenBook(path.toUtf8().constData(), "", OB_ADDTOLAST | OB_WITHRETURN);
    diagnostics::log(QString("OpenBook %1 -> %2").arg(path).arg(rc));
    return true;
}
