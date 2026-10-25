#include "bookorbitclient.h"
#include "accents.h"
#include "diagnostics.h"
#include "einkcolors.h"
#include "formats.h"

#include <inkview.h>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QUrl>

// TODO: make the server configurable
static const char *DEFAULT_SERVER_URL = "https://books.example.com";

// Books per request for the book list; pages load as the list scrolls to them
static const int BOOKS_PAGE_SIZE = 40;
// Books asked for at a time just to read their series numbers: the most the server sends
static const int SERIES_NUMBERS_PAGE_SIZE = 200;

// Searches start at this many characters: fewer match too much to be useful
static const int MIN_SEARCH_LENGTH = 3;

// Recent searches kept
static const int MAX_RECENT_SEARCHES = 10;

// Filter parts for BookOrbit's book queries
static QJsonObject rule(const QString &field, const QString &op, const QJsonValue &value)
{
    return QJsonObject{{"type", "rule"}, {"field", field}, {"operator", op}, {"value", value}};
}

static QJsonObject group(const QString &join, const QJsonArray &rules)
{
    return QJsonObject{{"type", "group"}, {"join", join}, {"rules", rules}};
}

BookOrbitClient::BookOrbitClient(const QString &settingsPath, QObject *parent)
    : QObject(parent)
    , m_settings(settingsPath, QSettings::IniFormat)
    , m_serverUrl(QString::fromUtf8(DEFAULT_SERVER_URL))
{
    m_nam.setTransferTimeout(30000);
    m_username = m_settings.value("username").toString();
    m_refreshToken = m_settings.value("refreshToken").toString();
    m_recentSearches = m_settings.value("recentSearches").toStringList();
    m_accent = m_settings.value("accent").toString();
    // A saved session counts as signed in straight away, so the app opens on the book list.
    // If the server has since rejected it, the first request finds out and signs out.
    m_signedIn = !m_refreshToken.isEmpty();
}

void BookOrbitClient::start()
{
    // The first request gets an access token from the saved session
    if (m_signedIn) {
        refreshBooks();
        loadAccent();
    }
}

QColor BookOrbitClient::accentColor() const
{
    // Made more vivid for the screen, like the covers
    return einkColor(accentColors(m_accent).color);
}

QColor BookOrbitClient::accentTextColor() const
{
    return accentColors(m_accent).text;
}

void BookOrbitClient::loadAccent()
{
    // {settings: {theme, accent, radius, background, brightness}}, or {settings: null} for
    // a user who has never synced their theme to the server. After they stop syncing, the
    // server keeps their last theme; that's good enough here. Quiet: the colour can wait.
    download("/user-preferences/theme",
        [this](const QByteArray &data) {
            const QString accent = QJsonDocument::fromJson(data).object().value("settings")
                                       .toObject().value("accent").toString();
            diagnostics::log("accent: " + (accent.isEmpty() ? QString("default") : accent));
            if (accent == m_accent)
                return;
            m_accent = accent;
            m_settings.setValue("accent", m_accent);
            m_settings.sync();
            emit accentChanged();
        },
        [](int, const QString &message) { diagnostics::log("accent: " + message); });
}

void BookOrbitClient::signIn(const QString &username, const QString &password)
{
    setError(QString());
    m_username = username.trimmed();
    emit usernameChanged();

    // Shown in the user's session list in BookOrbit
    const QString deviceLabel = QString::fromUtf8(GetDeviceModel()).trimmed();
    QJsonObject body{
        {"username", m_username},
        {"password", password},
        {"clientKind", "native"},
        {"deviceLabel", ("PocketBook " + deviceLabel).trimmed().left(100)},
    };
    // Returns NativeCredentials plus the user: {accessToken, refreshToken, sessionId, user, ...}
    send("POST", "/auth/login", QJsonDocument(body),
        [this](const QByteArray &data) {
            applyCredentials(QJsonDocument::fromJson(data).object());
            m_settings.setValue("username", m_username);
            m_settings.sync();
            refreshBooks();
            loadAccent();
        },
        [this](int, const QString &message) { setError(message); });
}

void BookOrbitClient::signOut()
{
    // Revoke the session on the server; the local sign-out doesn't wait for it
    if (!m_refreshToken.isEmpty()) {
        send("POST", "/auth/logout", QJsonDocument(QJsonObject{{"refreshToken", m_refreshToken}}),
             [](const QByteArray &) {}, [](int, const QString &) {});
    }
    clearSession();
    setError(QString());
    // They belong to this user
    m_recentSearches.clear();
    m_settings.remove("recentSearches");
    m_accent.clear();
    m_settings.remove("accent");
    m_settings.sync();
    emit recentSearchesChanged();
    emit accentChanged();
}

void BookOrbitClient::setBookQuery(const QString &sortField, bool descending,
                                   const QStringList &formats, bool onlyListed,
                                   const QVariantList &onlyBooks)
{
    m_list.sortField = sortField;
    m_list.sortDescending = descending;
    m_list.onlyListed = onlyListed;
    m_list.onlyBooks = onlyBooks;
    m_list.formats = formats;
    refreshBooks();
}

void BookOrbitClient::refreshBooks()
{
    refreshList(m_list);
}

void BookOrbitClient::refreshList(PagedBooks &list)
{
    restartPages(list);
    if (list.onlyListed) {
        showListedBooks(list);
        return;
    }
    PagedBooks *refreshed = &list;
    const int generation = list.queryGeneration;
    loadLibrary([this, refreshed, generation]() {
        if (generation != refreshed->queryGeneration)
            return;
        refreshed->query = bookQuery(*refreshed);
        if (refreshed == &m_list || m_booksOfField != "collection") {
            refreshed->path = libraryBooksPath();
            refreshed->jumpPath = refreshed->path + "/jump-buckets";
        }
        if (refreshed == &m_list) {
            // With every type listed, an audiobook's ebook record is in the list too
            refreshed->mergePairs = refreshed->formats.isEmpty();
        }
        // The A-Z sorts need their sections first: they say which rows start one
        if (isLetterSort(refreshed->sortField)) {
            refreshed->sections = Sections::Letters;
            loadJumpSections(*refreshed, [this, refreshed]() { loadPage(*refreshed, 0); });
        } else if (refreshed->sortField == "seriesIndex") {
            // Each book under its number in the series
            refreshed->sections = Sections::SeriesNumbers;
            clearJumpSections(*refreshed);
            loadPage(*refreshed, 0);
            loadSeriesSections(*refreshed);
        } else {
            // A collection's order has no sections: it doesn't say when each was added
            refreshed->sections = isDateSort(refreshed->sortField) ? Sections::Days : Sections::None;
            clearJumpSections(*refreshed);
            loadPage(*refreshed, 0);
        }
    });
}

void BookOrbitClient::search(const QString &text)
{
    restartPages(m_search);
    const QString q = text.trimmed();
    if (q.length() < MIN_SEARCH_LENGTH) {
        m_search.listGeneration = m_search.queryGeneration;
        m_search.total = 0;
        m_search.model.reset(0);
        emit searchChanged();
        return;
    }
    emit searchChanged();   // searchTotal is -1 until the results arrive
    const int generation = m_search.queryGeneration;
    loadLibrary([this, generation, q]() {
        if (generation != m_search.queryGeneration)
            return;
        // As BookOrbit's web client searches: the text as q, the results by title
        m_search.path = libraryBooksPath();
        m_search.query = QJsonObject{
            {"q", q},
            {"sort", QJsonArray{QJsonObject{{"field", "title"}, {"dir", "asc"}}}},
        };
        loadPage(m_search, 0);
    });
}

int BookOrbitClient::searchTotal() const
{
    return m_search.listGeneration == m_search.queryGeneration ? m_search.total : -1;
}

void BookOrbitClient::loadSearchResults(int first, int last)
{
    loadPages(m_search, first, last);
}

void BookOrbitClient::showBooksOf(const QString &field, const QStringList &values,
                                  const QString &sortField, bool descending,
                                  const QStringList &formats, bool onlyListed,
                                  const QVariantList &onlyBooks)
{
    // Other books: not the old rows meanwhile. The same ones sorted or filtered anew keep
    // them until the new rows arrive, as the library does.
    if (field != m_booksOfField || values != m_booksOfValues) {
        restartPages(m_booksOf);
        m_booksOf.listGeneration = -1;
        m_booksOf.total = 0;
        m_booksOf.model.reset(0);
        clearJumpSections(m_booksOf);
        emit booksOfChanged();
    }
    m_booksOfField = field;
    m_booksOfValues = values;
    m_booksOfCollectionIds.clear();
    if (field == "collection") {
        const int collectionId = values.value(0).toInt();
        for (const QVariantMap &row : std::as_const(m_collections.rows)) {
            if (row.value("id").toInt() == collectionId)
                m_booksOfCollectionIds = row.value("bookIds").toList();
        }
        const QString path = QString("/collections/%1/books").arg(collectionId);
        m_booksOf.path = path + "/query";
        m_booksOf.jumpPath = path + "/jump-buckets";
        m_booksOf.rules = QJsonArray();
        // An audiobook's ebook record may not be in the collection
        m_booksOf.mergePairs = false;
    } else {
        // The library's books, matching the names exactly. refreshList() sets the path.
        m_booksOf.rules = QJsonArray{rule(field, "includesAny", QJsonArray::fromStringList(values))};
        // With every type listed, an audiobook's ebook record is too
        m_booksOf.mergePairs = formats.isEmpty();
    }
    m_booksOf.sortField = sortField;
    m_booksOf.sortDescending = descending;
    m_booksOf.formats = formats;
    m_booksOf.onlyListed = onlyListed;
    // Just the downloaded books among these
    m_booksOf.onlyBooks.clear();
    for (const QVariant &value : onlyBooks) {
        const QVariantMap book = value.toMap();
        bool wanted = false;
        if (field == "collection") {
            wanted = m_booksOfCollectionIds.contains(book.value("id"));
        } else if (field == "series") {
            wanted = values.contains(book.value("seriesName").toString());
        } else {
            for (const QString &author : book.value("authors").toStringList())
                wanted = wanted || values.contains(author);
        }
        if (wanted)
            m_booksOf.onlyBooks.append(book);
    }
    refreshList(m_booksOf);
}

int BookOrbitClient::booksOfTotal() const
{
    return m_booksOf.listGeneration == m_booksOf.queryGeneration ? m_booksOf.total : -1;
}

void BookOrbitClient::loadBooksOf(int first, int last)
{
    loadPages(m_booksOf, first, last);
}

void BookOrbitClient::loadBookDetails(int bookId)
{
    // BookDetail (packages/types/src/book.ts). Quiet: the page shows the list's details
    // meanwhile, and the rest just doesn't appear offline.
    download(QString("/books/%1").arg(bookId),
        [this, bookId](const QByteArray &data) {
            emit bookDetails(bookId, QJsonDocument::fromJson(data).object().toVariantMap());
        },
        [this, bookId](int, const QString &message) {
            diagnostics::log(QString("book %1 details: %2").arg(bookId).arg(message));
            emit bookDetails(bookId, QVariantMap());
        });
}

void BookOrbitClient::addRecentSearch(const QString &text)
{
    const QString search = text.trimmed();
    if (search.length() < MIN_SEARCH_LENGTH)
        return;
    QStringList searches{search};
    for (const QString &recent : std::as_const(m_recentSearches)) {
        if (recent.compare(search, Qt::CaseInsensitive) != 0 && searches.size() < MAX_RECENT_SEARCHES)
            searches.append(recent);
    }
    if (searches == m_recentSearches)
        return;
    m_recentSearches = searches;
    m_settings.setValue("recentSearches", m_recentSearches);
    m_settings.sync();
    emit recentSearchesChanged();
}

void BookOrbitClient::loadLibrary(std::function<void()> then)
{
    if (m_libraryId >= 0) {
        then();
        return;
    }
    // [{id, name, bookCount, ...}] in the user's order.
    // TODO: choose the library when there is more than one; this shows the first.
    callJson("GET", "/libraries", QJsonDocument(), [this, then](const QJsonDocument &doc) {
        const QJsonArray libraries = doc.array();
        if (libraries.isEmpty()) {
            setError("No libraries on this server");
            return;
        }
        m_libraryId = libraries.first().toObject().value("id").toInt();
        then();
    });
}

// Just the letters and digits, in lower case: "Carl’s Doomsday Scenario" and "Carl's
// Doomsday Scenario" are the same, as are "James S.A. Corey" and "James S. A. Corey"
static QString looseText(const QString &text)
{
    QString loose;
    for (const QChar c : text) {
        if (c.isLetterOrNumber())
            loose.append(c.toLower());
    }
    return loose;
}

// The words of some text, without the punctuation around and inside them
static QStringList words(const QString &text)
{
    static const QRegularExpression notWord(R"([^\p{L}\p{N}]+)");
    return text.split(notWord, Qt::SkipEmptyParts);
}

// What makes two records the same book: title and first author, in loose text
static QString companionKey(const QVariantMap &book)
{
    const QStringList authors = book.value("authors").toStringList();
    return looseText(book.value("title").toString()) + '\n'
           + (authors.isEmpty() ? QString() : looseText(authors.first()));
}

// A record's versions of the book, at most one ebook and one audiobook:
// [{bookId, fileId, format, sizeBytes, kind: "ebook" | "audiobook", book}], book being the
// record, to download from. The primary file comes first; "content" files are the same
// book in other formats. Cover, metadata and supplement files aren't the book.
static QVariantList ownVariants(const QVariantMap &book)
{
    QVariantList variants;
    QStringList kinds;
    QVariantList files = book.value("files").toList();
    std::stable_sort(files.begin(), files.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("role") == "primary" && b.toMap().value("role") != "primary";
    });
    for (const QVariant &value : std::as_const(files)) {
        const QVariantMap file = value.toMap();
        const QString role = file.value("role").toString();
        const QString format = file.value("format").toString();
        if (format.isEmpty() || (role != "primary" && role != "content"))
            continue;
        const QString kind = mediaKind(format);
        if (kinds.contains(kind))
            continue;
        kinds.append(kind);
        variants.append(QVariantMap{
            {"bookId", book.value("id")},
            {"fileId", file.value("id")},
            {"format", format.toLower()},
            {"sizeBytes", file.value("sizeBytes")},
            {"kind", kind},
            {"book", book},
        });
    }
    return variants;
}

// Every book format a record has, primary first: "epub", "azw3", ...
static QStringList ownFormats(const QVariantMap &book)
{
    QStringList formats;
    for (const QVariant &value : book.value("files").toList()) {
        const QVariantMap file = value.toMap();
        const QString role = file.value("role").toString();
        const QString format = file.value("format").toString().toLower();
        if (format.isEmpty() || (role != "primary" && role != "content") || formats.contains(format))
            continue;
        if (role == "primary")
            formats.prepend(format);
        else
            formats.append(format);
    }
    return formats;
}

void BookOrbitClient::addCompanions(QVariantMap &row, const QList<QVariantMap> &companions, bool hideAudio)
{
    QVariantList variants = row.value("variants").toList();
    QStringList formats = row.value("formats").toStringList();
    QStringList kinds;
    for (const QVariant &variant : std::as_const(variants))
        kinds.append(variant.toMap().value("kind").toString());
    const QString ownKind = kinds.isEmpty() ? QString() : kinds.first();

    bool ebookElsewhere = false;
    for (const QVariantMap &companion : companions) {
        for (const QVariant &value : ownVariants(companion)) {
            const QString kind = value.toMap().value("kind").toString();
            if (kind == "ebook")
                ebookElsewhere = true;
            if (!kinds.contains(kind)) {
                kinds.append(kind);
                variants.append(value);
            }
        }
        for (const QString &format : ownFormats(companion)) {
            if (!formats.contains(format))
                formats.append(format);
        }
    }
    // Ebook versions first: a downloaded ebook opens before its audiobook
    std::stable_sort(variants.begin(), variants.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("kind") == "ebook" && b.toMap().value("kind") != "ebook";
    });
    row.insert("variants", variants);
    row.insert("formats", formats);
    row.insert("hidden", hideAudio && ownKind == "audiobook" && ebookElsewhere);
}

QJsonObject BookOrbitClient::bookQuery(const PagedBooks &list)
{
    QJsonObject query{
        {"sort", QJsonArray{QJsonObject{{"field", list.sortField}, {"dir", list.sortDescending ? "desc" : "asc"}}}},
    };

    QJsonArray rules = list.rules;
    if (!list.formats.isEmpty())
        rules.append(rule("format", "includesAny", QJsonArray::fromStringList(list.formats)));
    if (!rules.isEmpty())
        query.insert("filter", group("AND", rules));
    return query;
}

// "George R.R. Martin" -> "Martin, George R.R.": near enough to the sort names BookOrbit
// sorts authors by, which the list entries don't include
static QString authorSortKey(const QString &name)
{
    const int space = name.trimmed().lastIndexOf(' ');
    return space < 0 ? name.trimmed() : name.mid(space + 1) + ", " + name.left(space);
}

// "A" to "Z" for a key starting with a letter, otherwise "#", as BookOrbit's sections go
static QString letterSection(const QString &key)
{
    const QChar first = key.isEmpty() ? QChar() : key.at(0).toUpper();
    return first >= 'A' && first <= 'Z' ? QString(first) : QString("#");
}

void BookOrbitClient::showListedBooks(PagedBooks &list)
{
    const bool letters = isLetterSort(list.sortField);
    const bool seriesNumbers = list.sortField == "seriesIndex";
    list.sections = letters ? Sections::Letters
                  : seriesNumbers ? Sections::SeriesNumbers
                  : isDateSort(list.sortField) ? Sections::Days : Sections::None;
    // The key each book sorts by, as the server would sort it
    const QString field = list.sortField;
    const QVariantList collectionIds = &list == &m_booksOf ? m_booksOfCollectionIds : QVariantList();
    const auto sortKey = [field, collectionIds](const QVariantMap &book) -> QString {
        if (field == "author") {
            const QStringList authors = book.value("authors").toStringList();
            return authors.isEmpty() ? QString() : authorSortKey(authors.first());
        }
        if (field == "seriesIndex") {
            // Numbers in order, and books without one last
            const QVariant index = book.value("seriesIndex");
            return index.isNull() ? QString("~") : QString("%1").arg(index.toDouble(), 12, 'f', 3, QChar('0'));
        }
        if (field == "collectionOrder") {
            // Its place in the collection, counting from the first added
            const int latest = int(collectionIds.indexOf(book.value("id")));
            return QString("%1").arg(collectionIds.size() - latest, 8, 10, QChar('0'));
        }
        return book.value(field).toString();   // title, or an ISO date
    };

    // Records of the same book (an ebook and its audiobook kept apart) make one row, the
    // ebook's, with the other's formats added
    QHash<QString, QList<QVariantMap>> byKey;
    QStringList keys;   // in the order first seen
    for (const QVariant &value : std::as_const(list.onlyBooks)) {
        const QVariantMap book = value.toMap();
        const QString key = companionKey(book);
        if (!byKey.contains(key))
            keys.append(key);
        byKey[key].append(book);
    }
    QList<QPair<QString, QVariantMap>> books;   // sort key, row
    for (const QString &key : std::as_const(keys)) {
        QList<QVariantMap> records = byKey.value(key);
        // Shown if any of its files on the device is of the type chosen
        bool wanted = list.formats.isEmpty();
        for (const QVariantMap &record : std::as_const(records)) {
            for (const QString &format : record.value("downloadedFormats").toStringList())
                wanted = wanted || list.formats.contains(format);
        }
        if (!wanted)
            continue;
        int shown = 0;
        for (int i = 0; i < records.size(); i++) {
            const QVariantList variants = ownVariants(records.at(i));
            if (!variants.isEmpty() && variants.first().toMap().value("kind") == "ebook") {
                shown = i;
                break;
            }
        }
        const QVariantMap book = records.takeAt(shown);
        QVariantMap row = decorateBook(book, list);
        addCompanions(row, records, false);
        books.append({sortKey(book), row});
    }
    const bool descending = list.sortDescending;
    std::stable_sort(books.begin(), books.end(), [descending](const auto &a, const auto &b) {
        const int order = QString::localeAwareCompare(a.first.toLower(), b.first.toLower());
        return descending ? order > 0 : order < 0;
    });

    QVariantList rows;
    list.sectionStarts.clear();
    list.jumpSections.clear();
    for (const auto &[key, row] : std::as_const(books)) {
        if (letters) {
            const QString letter = letterSection(key);
            if (list.jumpSections.isEmpty() || list.jumpSections.last().toMap().value("label") != letter) {
                list.sectionStarts.insert(rows.size(), letter);
                list.jumpSections.append(QVariantMap{{"label", letter}, {"index", rows.size()}});
            }
        } else if (seriesNumbers && !row.value("seriesIndex").isNull()) {
            // As loadSeriesSections() lists them for the server's rows
            const QString label = row.value("seriesIndex").toString();
            if (list.jumpSections.isEmpty() || list.jumpSections.last().toMap().value("label") != label)
                list.jumpSections.append(QVariantMap{{"label", label}, {"index", rows.size()}});
        }
        rows.append(row);
    }

    // All there at once: no pages to load
    list.listGeneration = list.queryGeneration;
    list.total = rows.size();
    for (int page = 0; page * BOOKS_PAGE_SIZE < rows.size(); page++)
        list.loadedPages.insert(page);
    list.model.reset(rows.size());
    list.model.setBooks(0, rows);
    updateSectionStarts(list, 0, rows.size() - 1);
    emit jumpSectionsChanged();
    if (&list == &m_list)
        emit booksChanged();
    else
        emit booksOfChanged();
}

void BookOrbitClient::loadJumpSections(PagedBooks &list, std::function<void()> then)
{
    // As BookOrbit's web client asks for its jump rail: the list query plus maxBuckets.
    // Returns {buckets: [{key, label, index}], total, kind: "letter"}, index being the
    // section's first row. Sections follow the server's own sort keys (an author's sort
    // name, say), so they always match the list.
    QJsonObject query = list.query;
    query.insert("pagination", QJsonObject{{"page", 0}, {"size", BOOKS_PAGE_SIZE}});
    query.insert("maxBuckets", 64);
    PagedBooks *sectioned = &list;
    const int generation = list.queryGeneration;
    callJson("POST", list.jumpPath, QJsonDocument(query),
        [this, sectioned, generation, then](const QJsonDocument &doc) {
            if (generation != sectioned->queryGeneration)
                return;   // the query changed since this was asked for
            const QJsonObject result = doc.object();
            sectioned->sectionStarts.clear();
            sectioned->jumpSections.clear();
            if (result.value("kind").toString() == "letter") {
                for (const QJsonValue &bucket : result.value("buckets").toArray()) {
                    const QString label = bucket["label"].toString();
                    const int index = bucket["index"].toInt();
                    sectioned->sectionStarts.insert(index, label);
                    sectioned->jumpSections.append(QVariantMap{{"label", label}, {"index", index}});
                }
            }
            emit jumpSectionsChanged();
            then();
        });
}

void BookOrbitClient::loadSeriesSections(PagedBooks &list, int page)
{
    // The server has no jump buckets for the series order, so this reads every book's
    // number from the list itself, a large page at a time, while the rows load as usual
    QJsonObject query = list.query;
    query.insert("pagination", QJsonObject{{"page", page}, {"size", SERIES_NUMBERS_PAGE_SIZE}});
    PagedBooks *sectioned = &list;
    const int generation = list.queryGeneration;
    callJson("POST", list.path, QJsonDocument(query),
        [this, sectioned, page, generation](const QJsonDocument &doc) {
            if (generation != sectioned->queryGeneration)
                return;
            const QJsonObject result = doc.object();
            const QJsonArray items = result.value("items").toArray();
            for (int i = 0; i < items.size(); i++) {
                // Books with no number have no section. Records of the same book (an
                // ebook and its audiobook) share one.
                const QVariant index = items.at(i)["seriesIndex"].toVariant();
                if (index.isNull())
                    continue;
                const QString label = index.toString();
                if (sectioned->jumpSections.isEmpty() || sectioned->jumpSections.last().toMap().value("label") != label)
                    sectioned->jumpSections.append(QVariantMap{{"label", label}, {"index", page * SERIES_NUMBERS_PAGE_SIZE + i}});
            }
            if ((page + 1) * SERIES_NUMBERS_PAGE_SIZE < result.value("total").toInt() && !items.isEmpty())
                loadSeriesSections(*sectioned, page + 1);
            else
                emit jumpSectionsChanged();
        });
}

void BookOrbitClient::clearJumpSections(PagedBooks &list)
{
    list.sectionStarts.clear();
    list.jumpSections.clear();
    emit jumpSectionsChanged();
}

void BookOrbitClient::loadBooks(int first, int last)
{
    loadPages(m_list, first, last);
}

void BookOrbitClient::restartPages(PagedBooks &list)
{
    list.queryGeneration++;
    list.loadedPages.clear();
    list.loadingPages.clear();
}

void BookOrbitClient::loadPages(PagedBooks &list, int first, int last)
{
    // Pages of a new query wait for its first page, which sizes the list
    if (list.listGeneration != list.queryGeneration)
        return;
    first = qMax(0, first);
    last = qMin(last, list.model.count() - 1);
    for (int page = first / BOOKS_PAGE_SIZE; page <= last / BOOKS_PAGE_SIZE && last >= 0; page++) {
        if (!list.loadedPages.contains(page) && !list.loadingPages.contains(page))
            loadPage(list, page);
    }
}

void BookOrbitClient::loadPage(PagedBooks &list, int page)
{
    // Same query the web client sends: returns {items: [...], total, page, size}. Pages
    // count from 0, as in the web client: page 1 is the second page.
    QJsonObject query = list.query;
    query.insert("pagination", QJsonObject{{"page", page}, {"size", BOOKS_PAGE_SIZE}});

    list.loadingPages.insert(page);
    const int generation = list.queryGeneration;
    PagedBooks *pages = &list;
    callJson("POST", list.path, QJsonDocument(query),
        [this, pages, page, generation](const QJsonDocument &doc) {
            if (generation != pages->queryGeneration)
                return;   // the query changed since this was asked for
            pages->loadingPages.remove(page);
            pages->loadedPages.insert(page);
            const QJsonObject result = doc.object();
            // The first page of a new query replaces the list with one of the right length
            if (pages->listGeneration != generation) {
                pages->listGeneration = generation;
                pages->total = result.value("total").toInt();
                pages->model.reset(pages->total);
            }
            QVariantList books;
            QVariantList records;
            for (const QJsonValue &item : result.value("items").toArray()) {
                records.append(item.toObject().toVariantMap());
                books.append(decorateBook(records.last().toMap(), *pages));
            }
            const int first = page * BOOKS_PAGE_SIZE;
            pages->model.setBooks(first, books);
            lookUpCompanions(pages, first, records, generation);
            // Also the row after the page, whose section start depends on the last one here
            updateSectionStarts(*pages, first, first + int(books.size()));
            if (pages == &m_list) {
                emit booksChanged();
            } else if (pages == &m_booksOf) {
                emit booksOfChanged();
            } else {
                emit searchChanged();
            }
        },
        [pages, page, generation]() {
            if (generation == pages->queryGeneration)
                pages->loadingPages.remove(page);   // try again when it is next on screen
        });
}

void BookOrbitClient::lookUpCompanions(PagedBooks *list, int first, const QVariantList &books, int generation)
{
    if (books.isEmpty())
        return;
    // Every record that may have the same title and first author as a book on the page,
    // whatever the list's filter: any of (title has each of its words and an author has
    // the longest word of the first's). Punctuation is left out, as records can spell it
    // differently ("Carl’s" and "Carl's"); companionKey() then picks the matches.
    QJsonArray matches;
    for (const QVariant &value : books) {
        const QVariantMap book = value.toMap();
        const QString title = book.value("title").toString();
        QJsonArray match;
        for (const QString &word : words(title))
            match.append(rule("title", "contains", word));
        if (match.isEmpty())
            match.append(rule("title", "eq", title));
        const QStringList authors = book.value("authors").toStringList();
        QString authorWord;
        for (const QString &word : words(authors.value(0))) {
            if (word.length() > authorWord.length())
                authorWord = word;
        }
        if (!authorWord.isEmpty())
            match.append(rule("author", "includesAny", QJsonArray{authorWord}));
        matches.append(group("AND", match));
    }
    const QJsonObject query{
        {"filter", group("OR", matches)},
        {"pagination", QJsonObject{{"page", 0}, {"size", 4 * BOOKS_PAGE_SIZE}}},
    };
    callJson("POST", libraryBooksPath(), QJsonDocument(query),
        [this, list, first, books, generation](const QJsonDocument &doc) {
            if (generation != list->queryGeneration)
                return;
            QHash<QString, QList<QVariantMap>> byKey;
            for (const QJsonValue &item : doc.object().value("items").toArray()) {
                const QVariantMap record = item.toObject().toVariantMap();
                byKey[companionKey(record)].append(record);
            }
            for (int i = 0; i < books.size(); i++) {
                const QVariantMap book = books.at(i).toMap();
                QList<QVariantMap> companions;
                for (const QVariantMap &record : byKey.value(companionKey(book))) {
                    if (record.value("id") != book.value("id"))
                        companions.append(record);
                }
                if (companions.isEmpty())
                    continue;
                QVariantMap row = list->model.book(first + i);
                addCompanions(row, companions, list->mergePairs);
                // In a collection with every type listed, the audiobook record's row goes if
                // its ebook record is in the collection too, as the collections' counts go
                if (list == &m_booksOf && m_booksOfField == "collection" && list->formats.isEmpty()) {
                    QList<QVariantMap> inCollection;
                    for (const QVariantMap &companion : std::as_const(companions)) {
                        if (m_booksOfCollectionIds.contains(companion.value("id")))
                            inCollection.append(companion);
                    }
                    QVariantMap merged = list->model.book(first + i);
                    addCompanions(merged, inCollection, true);
                    row.insert("hidden", merged.value("hidden"));
                }
                for (const char *field : {"variants", "formats", "hidden"})
                    list->model.setField(first + i, field, row.value(field));
            }
            // Hidden rows pass their section boxes on to the next row
            updateSectionStarts(*list, first, first + int(books.size()));
        });
}

QVariantMap BookOrbitClient::decorateBook(const QVariantMap &book, const PagedBooks &list) const
{
    QVariantMap row = book;

    // Section, for the date sorts: the day. The A-Z sorts take theirs from the jump
    // sections instead (see updateSectionStarts).
    QString section;
    row.insert("sectionLabel", QString());
    if (list.sections == Sections::Days) {
        // Grouped by day, labelled like the system library: "2026/10/23", or "Never"
        const QString field = list.sortField == "lastReadAt" ? "lastReadAt" : "addedAt";
        const QDateTime when = QDateTime::fromString(book.value(field).toString(), Qt::ISODateWithMs);
        if (when.isValid()) {
            const QDate date = when.toLocalTime().date();
            section = date.toString("yyyy-MM-dd");
            row.insert("sectionLabel", date.toString("yyyy/MM/dd"));
        } else {
            section = "never";
            row.insert("sectionLabel", "Never");
        }
    }
    row.insert("section", section);
    row.insert("sectionStart", false);

    // Third line, as in the system library: reading status and series, either of which
    // may be missing. "19% read | The Rising World - 2", "Completed", "Discworld - 13".
    // Progress (0-100) counts whatever the status, as in BookOrbit's web app: reading
    // doesn't always set a status.
    const QString status = book.value("readStatus").toMap().value("status").toString();
    const double progress = book.value("readingProgress").toDouble();
    QString statusText;
    if (status == "read" || status == "skimmed" || progress >= 100) {
        statusText = "Completed";
    } else if (status == "on_hold") {
        statusText = "On hold";
    } else if (status == "abandoned") {
        statusText = "Abandoned";
    } else if (progress > 0) {
        // Never "0%" or "100%" for a book that is part read
        statusText = QString("%1% read").arg(qBound(1, qRound(progress), 99));
    } else if (status == "reading" || status == "rereading") {
        statusText = "Reading";
    }
    QString seriesText = book.value("seriesName").toString();
    if (!seriesText.isEmpty() && !book.value("seriesIndex").isNull())
        seriesText += " - " + book.value("seriesIndex").toString();
    row.insert("statusText", statusText);
    row.insert("seriesText", seriesText);

    // Its versions and formats; lookUpCompanions adds other records' later
    row.insert("variants", ownVariants(book));
    row.insert("formats", ownFormats(book));
    row.insert("hidden", false);
    return row;
}

void BookOrbitClient::updateSectionStarts(PagedBooks &list, int first, int last)
{
    if (list.sections == Sections::None)
        return;
    BookListModel &books = list.model;
    const auto hidden = [&books](int row) { return books.book(row).value("hidden").toBool(); };
    for (int row = qMax(0, first); row <= last && row < books.count(); row++) {
        if (!books.isLoaded(row))
            continue;
        if (hidden(row)) {
            books.setField(row, "sectionStart", false);
            continue;
        }
        // The visible row before: rows hidden in between don't count
        int previous = row - 1;
        while (previous >= 0 && books.isLoaded(previous) && hidden(previous))
            previous--;
        if (list.sections == Sections::SeriesNumbers) {
            // Its number in the series, as in seriesText, which the box stands in for
            const QVariant index = books.book(row).value("seriesIndex");
            books.setField(row, "sectionStart", !index.isNull());
            books.setField(row, "sectionLabel", index.isNull() ? QString() : index.toString());
            books.setField(row, "seriesText", QString());
        } else if (list.sections == Sections::Letters) {
            // A section starting here, or on the hidden rows just before
            QString label;
            for (int r = row; r > previous && label.isEmpty(); r--)
                label = list.sectionStarts.value(r);
            books.setField(row, "sectionStart", !label.isEmpty());
            books.setField(row, "sectionLabel", label);
        } else {
            // A new day, if the visible row before is known to be a different one. Until
            // that row loads, no box: its page fills this in.
            const QString section = books.book(row).value("section").toString();
            books.setField(row, "sectionStart", previous < 0
                || (books.isLoaded(previous) && books.book(previous).value("section").toString() != section));
        }
    }
}

// Ebooks before audiobooks, each in BookOrbit's order of formats
static void sortFormats(QStringList &formats)
{
    static const QStringList order{"epub", "kepub", "mobi", "azw3", "azw", "fb2", "pdf", "djvu",
                                   "cbz", "cbr", "m4b", "mp3", "m4a", "opus", "ogg", "flac"};
    std::stable_sort(formats.begin(), formats.end(), [](const QString &a, const QString &b) {
        return uint(order.indexOf(a)) < uint(order.indexOf(b));   // unknown ones last
    });
}

// By surname
static void sortAuthors(QStringList &authors)
{
    std::sort(authors.begin(), authors.end(), [](const QString &a, const QString &b) {
        return QString::localeAwareCompare(authorSortKey(a).toLower(), authorSortKey(b).toLower()) < 0;
    });
}

void BookOrbitClient::loadCollections()
{
    const int generation = ++m_collections.generation;
    // [{id, mediaType, name, icon, displayOrder, bookCount, ...}]: the user's own
    // collections and the public ones of others, in the user's order
    callJson("GET", "/collections", QJsonDocument(), [this, generation](const QJsonDocument &doc) {
        if (generation != m_collections.generation)
            return;
        m_collections.rows.clear();
        for (const QJsonValue &value : doc.array()) {
            QVariantMap row = value.toObject().toVariantMap();
            if (row.value("mediaType").toString() != "books")
                continue;   // podcast collections
            row.insert("covers", QVariantList());
            row.insert("authors", QStringList());
            row.insert("formats", QStringList());
            row.insert("bookIds", QVariantList());
            row.insert("bookKeys", QStringList());
            m_collections.rows.append(row);
        }
        m_collections.loaded = true;
        showGroups(m_collections);
        for (const QVariantMap &row : std::as_const(m_collections.rows)) {
            if (row.value("bookCount").toInt() > 0)
                loadCollectionBooks(row.value("id").toInt(), 0, generation);
        }
    });
}

void BookOrbitClient::loadCollectionBooks(int collectionId, int page, int generation)
{
    // Latest added first
    const QJsonObject query{
        {"sort", QJsonArray{QJsonObject{{"field", "collectionOrder"}, {"dir", "desc"}}}},
        {"pagination", QJsonObject{{"page", page}, {"size", SERIES_NUMBERS_PAGE_SIZE}}},
    };
    // Quiet: the rows are already there, and these just fill them in
    request("POST", QString("/collections/%1/books/query").arg(collectionId), QJsonDocument(query),
        [this, collectionId, page, generation](const QByteArray &data) {
            if (generation != m_collections.generation)
                return;
            const QJsonObject result = QJsonDocument::fromJson(data).object();
            const QJsonArray items = result.value("items").toArray();
            for (QVariantMap &row : m_collections.rows) {
                if (row.value("id").toInt() != collectionId)
                    continue;
                QVariantList covers = row.value("covers").toList();
                QStringList authors = row.value("authors").toStringList();
                QStringList formats = row.value("formats").toStringList();
                QVariantList bookIds = row.value("bookIds").toList();
                QStringList bookKeys = row.value("bookKeys").toStringList();
                for (const QJsonValue &item : items) {
                    const QVariantMap book = item.toObject().toVariantMap();
                    bookIds.append(book.value("id"));
                    // An ebook and its audiobook kept as separate records count once, as
                    // they make one row in the collection's books
                    const QString key = companionKey(book);
                    if (!bookKeys.contains(key))
                        bookKeys.append(key);
                    if (covers.size() < 2 && book.value("hasCover").toBool())
                        covers.append(QVariantMap{{"id", book.value("id")}, {"coverVersion", book.value("coverVersion")}});
                    for (const QString &author : book.value("authors").toStringList()) {
                        if (!authors.contains(author))
                            authors.append(author);
                    }
                    for (const QString &format : ownFormats(book)) {
                        if (!formats.contains(format))
                            formats.append(format);
                    }
                }
                sortAuthors(authors);
                sortFormats(formats);
                row.insert("covers", covers);
                row.insert("authors", authors);
                row.insert("formats", formats);
                row.insert("bookIds", bookIds);
                row.insert("bookKeys", bookKeys);
            }
            const bool more = (page + 1) * SERIES_NUMBERS_PAGE_SIZE < result.value("total").toInt() && !items.isEmpty();
            // Counted once all its books are in
            if (!more) {
                for (QVariantMap &row : m_collections.rows) {
                    if (row.value("id").toInt() == collectionId)
                        row.insert("bookCount", row.value("bookKeys").toStringList().size());
                }
            }
            showGroups(m_collections);
            if (more)
                loadCollectionBooks(collectionId, page + 1, generation);
        },
        [collectionId](int status, const QString &message) {
            diagnostics::log(QString("collection %1 books: %2 (%3)").arg(collectionId).arg(message).arg(status));
        });
}

void BookOrbitClient::setCollectionSort(const QString &field, bool descending)
{
    m_collections.sort = field;
    m_collections.descending = descending;
    showGroups(m_collections);
}

// Series listed at a time: the most the server sends
static const int SERIES_PAGE_SIZE = 100;
// The most values a filter rule takes, e.g. series names
static const int MAX_RULE_VALUES = 20;

void BookOrbitClient::loadSeries()
{
    const int generation = ++m_series.generation;
    loadLibrary([this, generation]() {
        if (generation == m_series.generation)
            loadSeriesPage(0, generation);
    });
}

void BookOrbitClient::loadSeriesPage(int page, int generation)
{
    // {items: [{id, name, bookCount, readCount, authors, coverBookIds (in the series'
    // order), lastAddedAt, ...}], total}
    const QString path = QString("/series?libraryId=%1&sort=name&order=asc&page=%2&size=%3")
                             .arg(m_libraryId).arg(page).arg(SERIES_PAGE_SIZE);
    callJson("GET", path, QJsonDocument(), [this, page, generation](const QJsonDocument &doc) {
        if (generation != m_series.generation)
            return;
        const QJsonObject result = doc.object();
        const QJsonArray items = result.value("items").toArray();
        // The old rows stay until the first page of new ones
        if (page == 0)
            m_series.rows.clear();
        for (const QJsonValue &value : items) {
            const QJsonObject series = value.toObject();
            QStringList authors = series.value("authors").toVariant().toStringList();
            sortAuthors(authors);
            // The list doesn't give the covers' versions; the cache keeps the first it got
            QVariantList covers;
            for (const QJsonValue &bookId : series.value("coverBookIds").toArray()) {
                if (covers.size() < 2)
                    covers.append(QVariantMap{{"id", bookId.toInt()}, {"coverVersion", QString()}});
            }
            const int bookCount = series.value("bookCount").toInt();
            m_series.rows.append(QVariantMap{
                {"id", series.value("id").toInt()},
                {"name", series.value("name").toString()},
                {"bookCount", bookCount},
                {"serverCount", bookCount},
                {"covers", covers},
                {"authors", authors},
                {"formats", QStringList()},
                {"books", QVariantMap()},
                {"lastAddedAt", series.value("lastAddedAt").toString()},
                {"readProgress", bookCount > 0 ? series.value("readCount").toDouble() / bookCount : 0.0},
            });
        }
        m_series.loaded = true;
        showGroups(m_series);
        if ((page + 1) * SERIES_PAGE_SIZE < result.value("total").toInt() && !items.isEmpty())
            loadSeriesPage(page + 1, generation);
        else
            loadGroupBooks(&m_series, 0, 0, generation);
    });
}

void BookOrbitClient::loadGroupBooks(Groups *groups, int first, int page, int generation)
{
    if (first >= groups->rows.size()) {
        groups->booksLoaded = true;
        showGroups(*groups);
        return;
    }
    // The books of as many series or authors as one request holds (at least one, and at
    // most as many as a rule takes), by name
    const bool series = groups == &m_series;
    QStringList names;
    int books = 0;
    int last = first;
    for (; last < groups->rows.size(); last++) {
        const int count = groups->rows.at(last).value("serverCount").toInt();
        if (!names.isEmpty() && (books + count > SERIES_NUMBERS_PAGE_SIZE || names.size() == MAX_RULE_VALUES))
            break;
        names.append(groups->rows.at(last).value("name").toString());
        books += count;
    }
    const QJsonObject query{
        {"filter", group("AND", {rule(series ? "series" : "author", "includesAny", QJsonArray::fromStringList(names))})},
        {"pagination", QJsonObject{{"page", page}, {"size", SERIES_NUMBERS_PAGE_SIZE}}},
    };
    // Quiet: the rows are already there, and these just fill them in
    request("POST", libraryBooksPath(), QJsonDocument(query),
        [this, groups, series, first, last, page, generation](const QByteArray &data) {
            if (generation != groups->generation)
                return;
            const QJsonObject result = QJsonDocument::fromJson(data).object();
            const QJsonArray items = result.value("items").toArray();
            // Each one's books: an ebook and its audiobook kept as separate records count
            // once, as they make one row in its book list, with the formats of both
            QHash<QString, QHash<QString, QStringList>> booksOf;   // name -> key -> formats
            for (const QJsonValue &item : items) {
                const QVariantMap book = item.toObject().toVariantMap();
                // Every series a book is in, or every author it has
                QStringList bookNames;
                if (series) {
                    for (const QVariant &membership : book.value("seriesMemberships").toList())
                        bookNames.append(membership.toMap().value("seriesName").toString());
                } else {
                    bookNames = book.value("authors").toStringList();
                }
                const QString key = companionKey(book);
                for (const QString &name : std::as_const(bookNames)) {
                    QStringList &formats = booksOf[name][key];
                    for (const QString &format : ownFormats(book)) {
                        if (!formats.contains(format))
                            formats.append(format);
                    }
                }
            }
            const bool more = (page + 1) * SERIES_NUMBERS_PAGE_SIZE < result.value("total").toInt() && !items.isEmpty();
            for (int i = first; i < last && i < groups->rows.size(); i++) {
                QVariantMap &row = groups->rows[i];
                QVariantMap rowBooks = row.value("books").toMap();
                QStringList formats = row.value("formats").toStringList();
                const QHash<QString, QStringList> found = booksOf.value(row.value("name").toString());
                for (auto it = found.constBegin(); it != found.constEnd(); ++it) {
                    QStringList bookFormats = rowBooks.value(it.key()).toStringList();
                    for (const QString &format : it.value()) {
                        if (!bookFormats.contains(format))
                            bookFormats.append(format);
                        if (!formats.contains(format))
                            formats.append(format);
                    }
                    rowBooks.insert(it.key(), bookFormats);
                }
                sortFormats(formats);
                row.insert("formats", formats);
                row.insert("books", rowBooks);
                // Counted once all its books are in
                if (!more && !rowBooks.isEmpty())
                    row.insert("bookCount", rowBooks.size());
            }
            showGroups(*groups);
            if (more)
                loadGroupBooks(groups, first, page + 1, generation);
            else
                loadGroupBooks(groups, last, 0, generation);
        },
        [](int status, const QString &message) {
            diagnostics::log(QString("series or author books: %1 (%2)").arg(message).arg(status));
        });
}

void BookOrbitClient::setSeriesSort(const QString &field, bool descending)
{
    m_series.sort = field;
    m_series.descending = descending;
    showGroups(m_series);
}

void BookOrbitClient::loadAuthors()
{
    const int generation = ++m_authors.generation;
    loadLibrary([this, generation]() {
        if (generation == m_authors.generation)
            loadAuthorsPage(0, generation);
    });
}

void BookOrbitClient::loadAuthorsPage(int page, int generation)
{
    // {items: [{id, name, sortName, bookCount, lastAddedAt, imageUrl (or null), ...}], total}
    const QString path = QString("/authors?libraryId=%1&sort=name&order=asc&page=%2&size=%3")
                             .arg(m_libraryId).arg(page).arg(SERIES_PAGE_SIZE);
    callJson("GET", path, QJsonDocument(), [this, page, generation](const QJsonDocument &doc) {
        if (generation != m_authors.generation)
            return;
        const QJsonObject result = doc.object();
        const QJsonArray items = result.value("items").toArray();
        // The old rows stay until the first page of new ones
        if (page == 0) {
            m_authors.rows.clear();
            m_authors.booksLoaded = false;
        }
        for (const QJsonValue &value : items) {
            const QJsonObject author = value.toObject();
            const QString name = author.value("name").toString();
            QString sortName = author.value("sortName").toString();
            if (sortName.isEmpty())
                sortName = authorSortKey(name);
            m_authors.rows.append(QVariantMap{
                {"kind", "author"},
                {"id", author.value("id").toInt()},
                {"name", name},
                {"sortName", sortName},
                {"bookCount", author.value("bookCount").toInt()},
                {"serverCount", author.value("bookCount").toInt()},
                {"photo", author.value("imageUrl").toString()},
                {"lastAddedAt", author.value("lastAddedAt").toString()},
                {"formats", QStringList()},
                {"books", QVariantMap()},
            });
        }
        m_authors.loaded = true;
        showGroups(m_authors);
        if ((page + 1) * SERIES_PAGE_SIZE < result.value("total").toInt() && !items.isEmpty())
            loadAuthorsPage(page + 1, generation);
        else
            loadGroupBooks(&m_authors, 0, 0, generation);
    });
}

void BookOrbitClient::setAuthorQuery(const QString &sortField, bool descending, const QStringList &formats,
                                     bool downloadedOnly, const QVariantList &downloadedBooks)
{
    m_authors.sort = sortField;
    m_authors.descending = descending;
    m_authors.formats = formats;
    m_authors.downloadedOnly = downloadedOnly;
    m_authors.downloadedBooks = downloadedBooks;
    showGroups(m_authors);
}

void BookOrbitClient::showGroups(Groups &groups)
{
    QList<QVariantMap> rows;
    if (&groups == &m_authors && (!groups.formats.isEmpty() || groups.downloadedOnly)) {
        // Only the authors with books of the type chosen (or such books on the device),
        // each counting just those and showing their formats, as the filtered book list
        // shows them. An author whose books haven't been looked up yet waits for them.
        QHash<QString, QHash<QString, QStringList>> downloaded;   // author -> key -> formats
        for (const QVariant &value : std::as_const(groups.downloadedBooks)) {
            const QVariantMap book = value.toMap();
            bool wanted = groups.formats.isEmpty();
            for (const QString &format : book.value("downloadedFormats").toStringList())
                wanted = wanted || groups.formats.contains(format);
            if (!wanted)
                continue;
            for (const QString &author : book.value("authors").toStringList()) {
                QStringList &formats = downloaded[author][companionKey(book)];
                for (const QString &format : ownFormats(book)) {
                    if (!formats.contains(format))
                        formats.append(format);
                }
            }
        }
        for (QVariantMap row : std::as_const(groups.rows)) {
            // Its books let through, by key, with their formats
            QHash<QString, QStringList> wantedBooks;
            if (groups.downloadedOnly) {
                wantedBooks = downloaded.value(row.value("name").toString());
            } else {
                const QVariantMap books = row.value("books").toMap();
                for (auto it = books.constBegin(); it != books.constEnd(); ++it) {
                    const QStringList formats = it.value().toStringList();
                    bool wanted = false;
                    for (const QString &format : formats)
                        wanted = wanted || groups.formats.contains(format);
                    if (wanted)
                        wantedBooks.insert(it.key(), formats);
                }
            }
            if (wantedBooks.isEmpty())
                continue;
            QStringList formats;
            for (const QStringList &bookFormats : std::as_const(wantedBooks)) {
                for (const QString &format : bookFormats) {
                    if (!formats.contains(format))
                        formats.append(format);
                }
            }
            sortFormats(formats);
            row.insert("bookCount", int(wantedBooks.size()));
            row.insert("formats", formats);
            rows.append(row);
        }
    } else {
        rows = groups.rows;
    }
    const QString field = groups.sort;
    std::stable_sort(rows.begin(), rows.end(), [field](const QVariantMap &a, const QVariantMap &b) {
        if (field == "bookCount")
            return a.value("bookCount").toInt() < b.value("bookCount").toInt();
        if (field == "readProgress")
            return a.value("readProgress").toDouble() < b.value("readProgress").toDouble();
        if (field == "lastAddedAt")   // "2026-09-17 13:03:35.074338+00": in order as text
            return a.value("lastAddedAt").toString() < b.value("lastAddedAt").toString();
        if (field == "sortName")   // an author's, "Martin, George R.R."
            return QString::localeAwareCompare(a.value("sortName").toString().toLower(), b.value("sortName").toString().toLower()) < 0;
        return QString::localeAwareCompare(a.value("name").toString().toLower(), b.value("name").toString().toLower()) < 0;
    });
    if (groups.descending)
        std::reverse(rows.begin(), rows.end());
    // By name, in A-Z sections like the library's titles
    QVariantList list;
    groups.jumpSections.clear();
    for (QVariantMap row : std::as_const(rows)) {
        QString label;
        if (field == "name" || field == "sortName") {
            const QString letter = letterSection(row.value(field).toString());
            if (groups.jumpSections.isEmpty() || groups.jumpSections.last().toMap().value("label") != letter) {
                groups.jumpSections.append(QVariantMap{{"label", letter}, {"index", list.size()}});
                label = letter;
            }
        }
        row.insert("sectionStart", !label.isEmpty());
        row.insert("sectionLabel", label);
        list.append(row);
    }
    // The same rows changing in place keep the list where it is
    if (groups.model.count() != list.size())
        groups.model.reset(list.size());
    groups.model.setBooks(0, list);
    if (&groups == &m_collections)
        emit collectionsChanged();
    else if (&groups == &m_series)
        emit seriesListChanged();
    else
        emit authorsChanged();
}

void BookOrbitClient::download(const QString &path, DataHandler onSuccess, ErrorHandler onError)
{
    call("GET", path, QJsonDocument(), onSuccess, onError, true);
}

void BookOrbitClient::request(const QByteArray &method, const QString &path, const QJsonDocument &body,
                              DataHandler onSuccess, ErrorHandler onError)
{
    call(method, path, body, onSuccess, onError, true);
}

void BookOrbitClient::withLibrary(std::function<void(int libraryId)> then, ErrorHandler onError)
{
    if (m_libraryId >= 0) {
        then(m_libraryId);
        return;
    }
    // As loadLibrary(), without touching the error property
    request("GET", "/libraries", QJsonDocument(),
        [this, then, onError](const QByteArray &data) {
            const QJsonArray libraries = QJsonDocument::fromJson(data).array();
            if (libraries.isEmpty()) {
                onError(0, "No libraries on this server");
                return;
            }
            m_libraryId = libraries.first().toObject().value("id").toInt();
            then(m_libraryId);
        },
        onError);
}

void BookOrbitClient::callJson(const QByteArray &method, const QString &path, const QJsonDocument &body,
                               std::function<void(const QJsonDocument &)> onSuccess,
                               std::function<void()> onFailed)
{
    call(method, path, body,
        [this, onSuccess, onFailed](const QByteArray &data) {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                setError("Unexpected response from server: " + parseError.errorString());
                if (onFailed)
                    onFailed();
                return;
            }
            onSuccess(doc);
        },
        [this, onFailed](int, const QString &message) {
            setError(message);
            if (onFailed)
                onFailed();
        });
}

void BookOrbitClient::call(const QByteArray &method, const QString &path, const QJsonDocument &body,
                           DataHandler onSuccess, ErrorHandler onError, bool quiet, bool retried)
{
    if (m_refreshing) {
        m_afterRefresh.append([=]() {
            if (m_signedIn)
                call(method, path, body, onSuccess, onError, quiet, retried);
        });
        return;
    }
    if (m_accessToken.isEmpty()) {
        if (m_refreshToken.isEmpty()) {
            onError(0, "Not signed in");
            return;
        }
        // Saved session, but no access token yet this run (or the last refresh failed,
        // e.g. offline): get one first. No retry if that fails, so no loop.
        refreshSession([=]() {
            if (m_signedIn && !m_accessToken.isEmpty())
                call(method, path, body, onSuccess, onError, quiet, retried);
        });
        return;
    }
    send(method, path, body, onSuccess, [=](int status, const QString &message) {
        if (status == 401 && !retried) {
            // Access tokens last about 15 minutes: get a new one and try once more
            refreshSession([=]() {
                if (m_signedIn)
                    call(method, path, body, onSuccess, onError, quiet, true);
            });
            return;
        }
        onError(status, message);
    }, true, quiet);
}

void BookOrbitClient::refreshSession(std::function<void()> then)
{
    m_afterRefresh.append(then);
    if (m_refreshing)
        return;
    m_refreshing = true;

    const auto finish = [this]() {
        m_refreshing = false;
        const auto queued = std::exchange(m_afterRefresh, {});
        for (const auto &fn : queued)
            fn();
    };
    // Returns NativeCredentials with a new refresh token; the old one stops working
    send("POST", "/auth/refresh", QJsonDocument(QJsonObject{{"refreshToken", m_refreshToken}}),
        [this, finish](const QByteArray &data) {
            applyCredentials(QJsonDocument::fromJson(data).object());
            finish();
        },
        [this, finish](int status, const QString &message) {
            // 401: the session has expired or was revoked. Anything else (offline, server
            // down) keeps the saved session so the next start can try again.
            if (status == 401) {
                clearSession();
                setError("Session expired, please sign in again");
            } else {
                setError(message);
            }
            finish();
        });
}

void BookOrbitClient::ensureNetwork()
{
#ifndef EMULATOR
    // Shows the system's own dialog while it connects
    if (!(QueryNetwork() & NET_CONNECTED)) {
        QElapsedTimer netTimer;
        netTimer.start();
        const int rc = NetConnect2(nullptr, 1);
        diagnostics::log(QString("NetConnect2 -> %1 after %2 ms").arg(rc).arg(netTimer.elapsed()));
    }
#endif
}

void BookOrbitClient::startStream(const QString &path, std::function<void(QNetworkReply *)> onStarted,
                                  ErrorHandler onError)
{
    if (m_refreshToken.isEmpty()) {
        onError(0, "Not signed in");
        return;
    }
    ensureNetwork();
    refreshSession([=]() {
        if (!m_signedIn || m_accessToken.isEmpty()) {
            onError(0, m_error.isEmpty() ? QString("Not signed in") : m_error);
            return;
        }
        QNetworkRequest request(QUrl(m_serverUrl + "/api/v1" + path));
        request.setRawHeader("Accept", "*/*");
        request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
        // A book can take a while: only give up if nothing arrives for a minute
        request.setTransferTimeout(60000);
        diagnostics::log("GET " + path + " (streamed)");
        onStarted(m_nam.get(request));
    });
}

void BookOrbitClient::send(const QByteArray &method, const QString &path, const QJsonDocument &body,
                           DataHandler onSuccess, ErrorHandler onError, bool withToken, bool quiet)
{
    // On the device, ask the system to bring up Wi-Fi first
    ensureNetwork();

    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1" + path));
    // Downloads (the only quiet requests) take whatever the server sends, e.g. a JPEG
    request.setRawHeader("Accept", quiet ? "*/*" : "application/json");
    if (withToken)
        request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    if (!body.isNull())
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam.sendCustomRequest(request, method, body.isNull() ? QByteArray() : body.toJson(QJsonDocument::Compact));
    if (!quiet) {
        m_pending++;
        emit busyChanged();
    }
    QElapsedTimer timer;
    timer.start();

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        // Clear busy only once the result (or error) has been applied
        const auto done = qScopeGuard([this, quiet] {
            if (!quiet) {
                m_pending--;
                emit busyChanged();
            }
        });

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        diagnostics::log(QString("%1 %2 -> %3 (%4 bytes) in %5 ms")
                             .arg(QString::fromLatin1(method), path)
                             .arg(status ? QString::number(status) : reply->errorString())
                             .arg(data.size()).arg(timer.elapsed()));
        if (reply->error() != QNetworkReply::NoError) {
            if (status == 0) {
                onError(0, reply->errorString());
                return;
            }
            // NestJS error body: {"statusCode": 401, "message": "Invalid credentials" | [...]}
            const QJsonValue message = QJsonDocument::fromJson(data).object().value("message");
            QString text = message.isArray() ? message.toArray().first().toString() : message.toString();
            if (text.isEmpty())
                text = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
            onError(status, QString("%1 (HTTP %2)").arg(text).arg(status));
            return;
        }

        onSuccess(data);
    });
}

void BookOrbitClient::applyCredentials(const QJsonObject &credentials)
{
    m_accessToken = credentials.value("accessToken").toString();
    m_refreshToken = credentials.value("refreshToken").toString();
    m_settings.setValue("refreshToken", m_refreshToken);
    m_settings.sync();
    setSignedIn(true);
}

void BookOrbitClient::clearSession()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_settings.remove("refreshToken");
    m_settings.sync();
    m_libraryId = -1;
    for (Groups *groups : {&m_collections, &m_series, &m_authors}) {
        groups->generation++;
        groups->rows.clear();
        groups->loaded = false;
        groups->model.reset(0);
        groups->jumpSections.clear();
    }
    emit collectionsChanged();
    emit seriesListChanged();
    emit authorsChanged();
    for (PagedBooks *list : {&m_list, &m_search, &m_booksOf}) {
        restartPages(*list);
        list->listGeneration = -1;
        list->total = 0;
        list->model.reset(0);
        list->sectionStarts.clear();
        list->jumpSections.clear();
    }
    emit jumpSectionsChanged();
    emit booksChanged();
    emit searchChanged();
    emit booksOfChanged();
    setSignedIn(false);
}

void BookOrbitClient::setSignedIn(bool signedIn)
{
    if (m_signedIn == signedIn)
        return;
    m_signedIn = signedIn;
    emit signedInChanged();
}

void BookOrbitClient::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
