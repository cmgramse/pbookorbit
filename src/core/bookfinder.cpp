#include "core/bookfinder.h"

#include "core/bookorbitclient.h"
#include "core/database.h"
#include "core/diagnostics.h"
#include "core/formats.h"

#include <inkview.h>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <cctype>

// Library books per request
static const int LIBRARY_PAGE_SIZE = 100;
// Candidates looked up per file
static const int MAX_CANDIDATES = 3;

// Folders to search: the app's download folders and the browser's downloads
static const char *SEARCH_DIRS[] = {FLASHDIR "/Books", AUDIOBOOKS_DEFAULT_PATH, DOWNLOADFOLDER};
static const char *KOREADER_SETTINGS_DIR = FLASHDIR "/applications/koreader/settings";

// Share of the progress bar for loading the library; file lookups take the rest
static const int LIBRARY_PERCENT = 30;

// Lower case, digits, and single spaces for anything else, with accents dropped: "The
// Emperor’s Legion: Book 1" -> "the emperor s legion book 1". Evens out characters file
// names can't hold (':' saved as '_').
static QString normalise(const QString &text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing)
            continue;
        out += c.isLetterOrNumber() ? c.toLower() : QChar(' ');
    }
    return out.simplified();
}

// Whether words occurs in text as whole words (both normalised)
static bool containsWords(const QString &text, const QString &words)
{
    return !words.isEmpty() && (' ' + text + ' ').contains(' ' + words + ' ');
}

// KOReader's partial MD5 (util.partialMD5), which BookOrbit stores as each file's hash: 1
// KB at offset 0, then at 1 KB, 4 KB, 16 KB and so on up to 1 GB
static QString partialMd5(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Md5);
    const qint64 size = file.size();
    for (int i = -1; i <= 10; i++) {
        const qint64 position = i < 0 ? 0 : qint64(1024) << (2 * i);
        if (position >= size)
            break;
        file.seek(position);
        hash.addData(file.read(1024));
    }
    return QString::fromLatin1(hash.result().toHex());
}

// Parses KOReader's LuaSettings files: "return { ... }" with tables, strings, numbers and
// booleans. Tables become maps with text keys; anything unrecognised becomes an invalid
// QVariant.
class LuaData
{
public:
    explicit LuaData(const QByteArray &text) : m_text(text) {}

    QVariant read()
    {
        skipSpace();
        if (m_text.mid(m_pos, 6) == "return")
            m_pos += 6;
        return value();
    }

private:
    void skipSpace()
    {
        while (m_pos < m_text.size()) {
            const char c = m_text.at(m_pos);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                m_pos++;
            } else if (m_text.mid(m_pos, 2) == "--") {
                while (m_pos < m_text.size() && m_text.at(m_pos) != '\n')
                    m_pos++;
            } else {
                break;
            }
        }
    }

    bool take(char c)
    {
        skipSpace();
        if (m_pos < m_text.size() && m_text.at(m_pos) == c) {
            m_pos++;
            return true;
        }
        return false;
    }

    QVariant value()
    {
        skipSpace();
        if (m_pos >= m_text.size())
            return QVariant();
        const char c = m_text.at(m_pos);
        if (c == '{')
            return table();
        if (c == '"' || c == '\'')
            return string();
        for (const char *word : {"true", "false", "nil"}) {
            const int length = int(qstrlen(word));
            if (m_text.mid(m_pos, length) == word) {
                m_pos += length;
                return word[0] == 'n' ? QVariant() : QVariant(word[0] == 't');
            }
        }
        return number();
    }

    QVariant table()
    {
        m_pos++;   // {
        QVariantMap map;
        int nextIndex = 1;
        while (!take('}')) {
            if (m_pos >= m_text.size())
                return QVariant();
            QString key;
            if (take('[')) {
                key = value().toString();
                if (!take(']') || !take('='))
                    return QVariant();
            } else {
                // name = value, or a value in the list part
                const int start = m_pos;
                while (m_pos < m_text.size() && (std::isalnum(uchar(m_text.at(m_pos))) || m_text.at(m_pos) == '_'))
                    m_pos++;
                const QByteArray name = m_text.mid(start, m_pos - start);
                if (!name.isEmpty() && !std::isdigit(uchar(name.at(0))) && take('=')) {
                    key = QString::fromLatin1(name);
                } else {
                    m_pos = start;
                    key = QString::number(nextIndex++);
                }
            }
            map.insert(key, value());
            if (!take(',') && !take(';')) {
                if (!take('}'))
                    return QVariant();
                break;
            }
        }
        return map;
    }

    QVariant string()
    {
        const char quote = m_text.at(m_pos++);
        QByteArray bytes;
        while (m_pos < m_text.size()) {
            char c = m_text.at(m_pos++);
            if (c == quote)
                return QString::fromUtf8(bytes);
            if (c == '\\' && m_pos < m_text.size()) {
                c = m_text.at(m_pos++);
                if (c >= '0' && c <= '9') {
                    // \ddd, up to three decimal digits
                    int code = c - '0';
                    for (int n = 0; n < 2 && m_pos < m_text.size() && std::isdigit(uchar(m_text.at(m_pos))); n++)
                        code = code * 10 + (m_text.at(m_pos++) - '0');
                    c = char(code);
                } else {
                    switch (c) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    default: break;   // \\, \", \' and an escaped line break stand for themselves
                    }
                }
            }
            bytes += c;
        }
        return QVariant();
    }

    QVariant number()
    {
        const int start = m_pos;
        while (m_pos < m_text.size() && QByteArray("+-.0123456789eExXabcdefABCDEF").contains(m_text.at(m_pos)))
            m_pos++;
        bool ok = false;
        const double number = m_text.mid(start, m_pos - start).toDouble(&ok);
        if (!ok) {
            m_pos = int(m_text.size());   // stop: not something this reads
            return QVariant();
        }
        return number;
    }

    const QByteArray m_text;
    int m_pos = 0;
};

BookFinder::BookFinder(BookOrbitClient *client, Downloads *downloads, Database *database,
                       QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_downloads(downloads)
    , m_database(database)
{
}

QVariantList BookFinder::unsure() const
{
    QVariantList list;
    for (const Downloads::Existing &book : m_unsure)
        list.append(QVariantMap{{"fileName", QFileInfo(book.path).fileName()},
                                {"title", book.entry.value("title")}});
    return list;
}

QList<BookFinder::LocalFile> BookFinder::unrecordedFiles() const
{
    const QSet<QString> recorded = m_downloads->recordedPaths();
    QList<LocalFile> files;
    for (const char *dir : SEARCH_DIRS) {
        const QString root = QString::fromUtf8(dir);
        // Skip hidden files, hidden folders and KOReader's .sdr folders
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString relative = path.mid(root.size());
            if (relative.contains("/.") || relative.contains(".sdr/") || recorded.contains(path))
                continue;
            const QFileInfo info(path);
            const QString format = info.suffix().toLower();
            if (!isBookFormat(format))
                continue;
            LocalFile file;
            file.path = path;
            file.fileName = info.fileName();
            file.format = format;
            file.nameKey = normalise(info.completeBaseName());
            files.append(file);
        }
    }
    return files;
}

int BookFinder::unrecordedCount() const
{
    return int(unrecordedFiles().size());
}

bool BookFinder::shouldOffer() const
{
    return !m_database->flag("findBooksOffered") && unrecordedCount() > 0;
}

void BookFinder::markOffered()
{
    m_database->setFlag("findBooksOffered", true);
}

QHash<QString, BookFinder::KoreaderMatch> BookFinder::koreaderMatches() const
{
    // One state file per KOReader account: bookorbit_sync_state.lua, plus
    // bookorbit_sync_state_<md5>.lua for others. Skip the .old backups.
    static const QRegularExpression stateName(R"(^bookorbit_sync_state(_[0-9a-f]{32})?\.lua$)");
    // KOReader's account id is "<server>/api/v1|<KOReader user>". That user is the KOReader
    // sync login, not necessarily the BookOrbit one, so only the server is compared;
    // matched books must still be in this user's library.
    const QString server = m_client->serverUrl() + "/api/v1|";
    QHash<QString, KoreaderMatch> matches;
    const QDir dir(QString::fromUtf8(KOREADER_SETTINGS_DIR));
    for (const QString &name : dir.entryList(QDir::Files)) {
        if (!stateName.match(name).hasMatch())
            continue;
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QVariantMap state = LuaData(file.readAll()).read().toMap();
        const QString account = state.value("account_identity").toString();
        if (!account.startsWith(server, Qt::CaseInsensitive)) {
            diagnostics::trace(QString("find books: %1 is for %2, skipped").arg(name, account));
            continue;
        }
        // books: {<partial MD5>: {bookId, fileId, file, ...}}, where file is the device
        // path (/mnt/ext1/...; FLASHDIR in the emulator)
        const QVariantMap books = state.value("books").toMap();
        for (auto it = books.cbegin(); it != books.cend(); ++it) {
            const QVariantMap book = it.value().toMap();
            QString path = book.value("file").toString();
            if (path.startsWith("/mnt/ext1/"))
                path = QString::fromUtf8(FLASHDIR) + path.mid(9);
            if (!path.isEmpty() && book.contains("bookId"))
                matches.insert(path, KoreaderMatch{book.value("bookId").toInt(), it.key()});
        }
        diagnostics::trace(QString("find books: %1 lists %2 books").arg(name).arg(books.size()));
    }
    return matches;
}

void BookFinder::start()
{
    if (m_state == "searching")
        return;
    const int generation = ++m_generation;
    m_state = "searching";
    m_percent = 0;
    m_error.clear();
    m_foundCount = 0;
    m_notFoundCount = 0;
    m_found.clear();
    m_unsure.clear();
    m_pending.clear();
    m_library.clear();
    m_titleKeys.clear();
    m_authorKeys.clear();
    m_libraryIndex.clear();
    m_libraryTotal = 0;
    emit changed();

    m_files = unrecordedFiles();
    for (LocalFile &file : m_files)
        file.md5 = partialMd5(file.path);
    m_koreader = koreaderMatches();
    diagnostics::trace(QString("find books: %1 unrecorded files").arg(m_files.size()));
    if (m_files.isEmpty()) {
        finish(generation);
        return;
    }
    m_client->withLibrary(
        [this, generation](int libraryId) {
            if (generation == m_generation)
                loadLibraryPage(libraryId, 0, generation);
        },
        [this, generation](int, const QString &message) {
            if (generation == m_generation)
                fail(message);
        });
}

void BookFinder::loadLibraryPage(int libraryId, int page, int generation)
{
    const QJsonObject query{
        {"sort", QJsonArray{QJsonObject{{"field", "title"}, {"dir", "asc"}}}},
        {"pagination", QJsonObject{{"page", page}, {"size", LIBRARY_PAGE_SIZE}}},
    };
    m_client->request("POST", QString("/libraries/%1/books").arg(libraryId), QJsonDocument(query),
        [this, libraryId, page, generation](const QByteArray &data) {
            if (generation != m_generation)
                return;
            const QJsonObject reply = QJsonDocument::fromJson(data).object();
            const QJsonArray items = reply.value("items").toArray();
            m_libraryTotal = reply.value("total").toInt();
            for (const QJsonValue &item : items) {
                const QVariantMap book = item.toObject().toVariantMap();
                m_libraryIndex.insert(book.value("id").toInt(), int(m_library.size()));
                m_library.append(book);
                m_titleKeys.append(normalise(book.value("title").toString()));
                const QStringList authors = book.value("authors").toStringList();
                const QStringList names = authors.isEmpty() ? QStringList()
                                          : normalise(authors.first()).split(' ', Qt::SkipEmptyParts);
                m_authorKeys.append(names.isEmpty() ? QString() : names.last());
            }
            if (m_libraryTotal > 0)
                setPercent(LIBRARY_PERCENT * int(m_library.size()) / m_libraryTotal);
            if (!items.isEmpty() && m_library.size() < m_libraryTotal)
                loadLibraryPage(libraryId, page + 1, generation);
            else
                matchFiles(generation);
        },
        [this, generation](int, const QString &message) {
            if (generation == m_generation)
                fail(message);
        });
}

void BookFinder::matchFiles(int generation)
{
    diagnostics::trace(QString("find books: library has %1 books").arg(m_library.size()));
    for (LocalFile &file : m_files) {
        // 1. Known to KOReader's plugin, file unchanged, and the book is in the library
        const auto koreader = m_koreader.constFind(file.path);
        if (koreader != m_koreader.cend() && koreader->md5 == file.md5
                && m_libraryIndex.contains(koreader->bookId)) {
            m_found.append({koreader->bookId, file.path, m_library.at(m_libraryIndex.value(koreader->bookId))});
            continue;
        }
        // Otherwise, books in the file's format whose title is in its name. Those with the
        // author in the name come first, then longer titles (Dune Messiah before Dune).
        QList<QPair<int, int>> scored;   // score, index
        for (int i = 0; i < m_library.size(); i++) {
            if (!containsWords(file.nameKey, m_titleKeys.at(i)))
                continue;
            bool hasFormat = false;
            for (const QVariant &value : m_library.at(i).value("files").toList())
                hasFormat = hasFormat || value.toMap().value("format").toString().toLower() == file.format;
            if (!hasFormat)
                continue;
            const bool author = containsWords(file.nameKey, m_authorKeys.at(i));
            scored.append({(author ? 100000 : 0) + int(m_titleKeys.at(i).size()), i});
        }
        std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        for (int n = 0; n < scored.size() && n < MAX_CANDIDATES; n++)
            file.candidates.append(scored.at(n).second);
        if (file.candidates.isEmpty())
            m_notFoundCount++;
        else
            m_pending.append(file);
    }
    checkCandidate(0, 0, generation);
}

void BookFinder::checkCandidate(int index, int candidate, int generation)
{
    if (index >= m_pending.size()) {
        finish(generation);
        return;
    }
    setPercent(LIBRARY_PERCENT + (100 - LIBRARY_PERCENT) * index / int(m_pending.size()));
    const LocalFile &file = m_pending.at(index);
    if (candidate >= file.candidates.size()) {
        // 4. None of their files is this one: the best candidate is a guess for the user to
        // confirm. It may be an older copy of the right book, since BookOrbit can write
        // metadata into its files, changing both their hash and their name.
        const int best = file.candidates.first();
        m_unsure.append({m_library.at(best).value("id").toInt(), file.path, m_library.at(best)});
        checkCandidate(index + 1, 0, generation);
        return;
    }
    const QVariantMap book = m_library.at(file.candidates.at(candidate));
    const int bookId = book.value("id").toInt();
    // Book details: files [{id, format, filename, fileHash, ...}]
    m_client->request("GET", QString("/books/%1").arg(bookId), QJsonDocument(),
        [this, index, candidate, generation, book, bookId](const QByteArray &data) {
            if (generation != m_generation)
                return;
            const LocalFile &file = m_pending.at(index);
            for (const QJsonValue &value : QJsonDocument::fromJson(data).object().value("files").toArray()) {
                const QJsonObject serverFile = value.toObject();
                // 2. Same bytes, as far as KOReader's hash can tell; 3. same name
                if (serverFile.value("fileHash").toString() == file.md5
                        || serverFile.value("filename").toString().compare(file.fileName, Qt::CaseInsensitive) == 0) {
                    m_found.append({bookId, file.path, book});
                    checkCandidate(index + 1, 0, generation);
                    return;
                }
            }
            checkCandidate(index, candidate + 1, generation);
        },
        [this, index, candidate, generation](int status, const QString &message) {
            if (generation != m_generation)
                return;
            // A book deleted since the list loaded just isn't a match; any other error
            // stops the search
            if (status == 404 || status == 403)
                checkCandidate(index, candidate + 1, generation);
            else
                fail(message);
        });
}

void BookFinder::finish(int generation)
{
    if (generation != m_generation)
        return;
    m_downloads->addExisting(m_found);
    m_foundCount = int(m_found.size());
    diagnostics::trace(QString("find books: %1 found, %2 unsure, %3 not found")
                         .arg(m_foundCount).arg(m_unsure.size()).arg(m_notFoundCount));
    for (const Downloads::Existing &book : m_unsure)
        diagnostics::trace(QString("find books: unsure: %1 -> book %2").arg(book.path).arg(book.bookId));
    m_files.clear();
    m_pending.clear();
    m_library.clear();
    m_titleKeys.clear();
    m_authorKeys.clear();
    m_libraryIndex.clear();
    m_state = "done";
    m_percent = 100;
    emit changed();
}

void BookFinder::fail(const QString &error)
{
    m_generation++;
    m_state = "failed";
    m_error = error;
    diagnostics::log("find books failed: " + error);
    emit changed();
}

void BookFinder::cancel()
{
    m_generation++;
    m_state = "idle";
    emit changed();
}

void BookFinder::setPercent(int percent)
{
    if (percent == m_percent)
        return;
    m_percent = percent;
    emit changed();
}
