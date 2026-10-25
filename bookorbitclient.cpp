#include "bookorbitclient.h"
#include "diagnostics.h"

#include <inkview.h>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QUrl>

// TODO: make the server configurable
static const char *DEFAULT_SERVER_URL = "https://books.example.com";

// Books per request for the book list; pages load as the list scrolls to them
static const int BOOKS_PAGE_SIZE = 40;

// Searches start at this many characters: fewer match too much to be useful
static const int MIN_SEARCH_LENGTH = 3;

// Recent searches kept
static const int MAX_RECENT_SEARCHES = 10;

BookOrbitClient::BookOrbitClient(const QString &settingsPath, QObject *parent)
    : QObject(parent)
    , m_settings(settingsPath, QSettings::IniFormat)
    , m_serverUrl(QString::fromUtf8(DEFAULT_SERVER_URL))
{
    m_nam.setTransferTimeout(30000);
    m_username = m_settings.value("username").toString();
    m_refreshToken = m_settings.value("refreshToken").toString();
    m_recentSearches = m_settings.value("recentSearches").toStringList();
    // A saved session counts as signed in straight away, so the app opens on the book list.
    // If the server has since rejected it, the first request finds out and signs out.
    m_signedIn = !m_refreshToken.isEmpty();
}

void BookOrbitClient::start()
{
    // The first request gets an access token from the saved session
    if (m_signedIn)
        refreshBooks();
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
    m_settings.sync();
    emit recentSearchesChanged();
}

void BookOrbitClient::setBookQuery(const QString &sortField, bool descending,
                                   const QStringList &formats, bool onlyListed,
                                   const QVariantList &onlyBooks)
{
    m_sortField = sortField;
    m_sortDescending = descending;
    m_onlyListed = onlyListed;
    m_onlyBooks = onlyBooks;
    m_formats = formats;
    refreshBooks();
}

void BookOrbitClient::refreshBooks()
{
    restartPages(m_list);
    if (m_onlyListed) {
        showListedBooks();
        return;
    }
    const int generation = m_list.queryGeneration;
    loadLibrary([this, generation]() {
        if (generation != m_list.queryGeneration)
            return;
        m_list.query = bookQuery();
        // The A-Z sorts need their sections first: they say which rows start one
        if (letterSections()) {
            loadJumpSections([this, generation]() {
                if (generation == m_list.queryGeneration)
                    loadPage(m_list, 0);
            });
        } else {
            m_sectionStarts.clear();
            m_jumpSections.clear();
            emit jumpSectionsChanged();
            loadPage(m_list, 0);
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

QJsonObject BookOrbitClient::bookQuery() const
{
    QJsonObject query{
        {"sort", QJsonArray{QJsonObject{{"field", m_sortField}, {"dir", m_sortDescending ? "desc" : "asc"}}}},
    };
    const auto rule = [](const QString &field, const QString &op, const QJsonValue &value) {
        return QJsonObject{{"type", "rule"}, {"field", field}, {"operator", op}, {"value", value}};
    };
    const auto group = [](const QString &join, const QJsonArray &rules) {
        return QJsonObject{{"type", "group"}, {"join", join}, {"rules", rules}};
    };

    if (!m_formats.isEmpty())
        query.insert("filter", group("AND", {rule("format", "includesAny", QJsonArray::fromStringList(m_formats))}));
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

void BookOrbitClient::showListedBooks()
{
    // The key each book sorts by, as the server would sort it
    const auto sortKey = [this](const QVariantMap &book) -> QString {
        if (m_sortField == "author") {
            const QStringList authors = book.value("authors").toStringList();
            return authors.isEmpty() ? QString() : authorSortKey(authors.first());
        }
        return book.value(m_sortField).toString();   // title, or an ISO date
    };

    QList<QPair<QString, QVariantMap>> books;
    for (const QVariant &value : std::as_const(m_onlyBooks)) {
        const QVariantMap book = value.toMap();
        if (m_formats.isEmpty() || m_formats.contains(book.value("downloadedFormat").toString()))
            books.append({sortKey(book), book});
    }
    std::stable_sort(books.begin(), books.end(), [this](const auto &a, const auto &b) {
        const int order = QString::localeAwareCompare(a.first.toLower(), b.first.toLower());
        return m_sortDescending ? order > 0 : order < 0;
    });

    QVariantList rows;
    m_sectionStarts.clear();
    m_jumpSections.clear();
    for (const auto &[key, book] : std::as_const(books)) {
        if (letterSections()) {
            const QString letter = letterSection(key);
            if (m_jumpSections.isEmpty() || m_jumpSections.last().toMap().value("label") != letter) {
                m_sectionStarts.insert(rows.size(), letter);
                m_jumpSections.append(QVariantMap{{"label", letter}, {"index", rows.size()}});
            }
        }
        rows.append(decorateBook(book));
    }

    // All there at once: no pages to load
    m_list.listGeneration = m_list.queryGeneration;
    m_list.total = rows.size();
    for (int page = 0; page * BOOKS_PAGE_SIZE < rows.size(); page++)
        m_list.loadedPages.insert(page);
    m_list.model.reset(rows.size());
    m_list.model.setBooks(0, rows);
    updateSectionStarts(0, rows.size() - 1);
    emit jumpSectionsChanged();
    emit booksChanged();
}

void BookOrbitClient::loadJumpSections(std::function<void()> then)
{
    // As BookOrbit's web client asks for its jump rail: the list query plus maxBuckets.
    // Returns {buckets: [{key, label, index}], total, kind: "letter"}, index being the
    // section's first row. Sections follow the server's own sort keys (an author's sort
    // name, say), so they always match the list.
    QJsonObject query = bookQuery();
    query.insert("pagination", QJsonObject{{"page", 0}, {"size", BOOKS_PAGE_SIZE}});
    query.insert("maxBuckets", 64);
    callJson("POST", QString("/libraries/%1/books/jump-buckets").arg(m_libraryId), QJsonDocument(query),
        [this, then](const QJsonDocument &doc) {
            const QJsonObject result = doc.object();
            m_sectionStarts.clear();
            m_jumpSections.clear();
            if (result.value("kind").toString() == "letter") {
                for (const QJsonValue &bucket : result.value("buckets").toArray()) {
                    const QString label = bucket["label"].toString();
                    const int index = bucket["index"].toInt();
                    m_sectionStarts.insert(index, label);
                    m_jumpSections.append(QVariantMap{{"label", label}, {"index", index}});
                }
            }
            emit jumpSectionsChanged();
            then();
        });
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
    callJson("POST", QString("/libraries/%1/books").arg(m_libraryId), QJsonDocument(query),
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
            for (const QJsonValue &item : result.value("items").toArray())
                books.append(decorateBook(item.toObject().toVariantMap()));
            const int first = page * BOOKS_PAGE_SIZE;
            pages->model.setBooks(first, books);
            if (pages == &m_list) {
                // Also the row after the page, whose section start depends on the last one here
                updateSectionStarts(first, first + int(books.size()));
                emit booksChanged();
            } else {
                emit searchChanged();
            }
        },
        [pages, page, generation]() {
            if (generation == pages->queryGeneration)
                pages->loadingPages.remove(page);   // try again when it is next on screen
        });
}

QVariantMap BookOrbitClient::decorateBook(const QVariantMap &book) const
{
    QVariantMap row = book;

    // Section, for the date sorts: the day. The A-Z sorts take theirs from the jump
    // sections instead (see updateSectionStarts).
    QString section;
    row.insert("sectionLabel", QString());
    if (!letterSections()) {
        // Grouped by day, labelled like the system library: "2026/10/23", or "Never"
        const QString field = m_sortField == "lastReadAt" ? "lastReadAt" : "addedAt";
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

    return row;
}

void BookOrbitClient::updateSectionStarts(int first, int last)
{
    BookListModel &books = m_list.model;
    for (int row = qMax(0, first); row <= last && row < books.count(); row++) {
        if (!books.isLoaded(row))
            continue;
        if (letterSections()) {
            books.setField(row, "sectionStart", m_sectionStarts.contains(row));
            books.setField(row, "sectionLabel", m_sectionStarts.value(row));
        } else {
            // A new day, if the row before is known to be a different one. Until that row
            // loads, no box: its page fills this in.
            const QString section = books.book(row).value("section").toString();
            books.setField(row, "sectionStart", row == 0
                || (books.isLoaded(row - 1) && books.book(row - 1).value("section").toString() != section));
        }
    }
}

void BookOrbitClient::download(const QString &path, DataHandler onSuccess, ErrorHandler onError)
{
    call("GET", path, QJsonDocument(), onSuccess, onError, true);
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
    for (PagedBooks *list : {&m_list, &m_search}) {
        restartPages(*list);
        list->listGeneration = -1;
        list->total = 0;
        list->model.reset(0);
    }
    m_sectionStarts.clear();
    m_jumpSections.clear();
    emit jumpSectionsChanged();
    emit booksChanged();
    emit searchChanged();
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
