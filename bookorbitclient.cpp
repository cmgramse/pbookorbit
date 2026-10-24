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

BookOrbitClient::BookOrbitClient(const QString &settingsPath, QObject *parent)
    : QObject(parent)
    , m_settings(settingsPath, QSettings::IniFormat)
    , m_serverUrl(QString::fromUtf8(DEFAULT_SERVER_URL))
{
    m_nam.setTransferTimeout(30000);
    m_username = m_settings.value("username").toString();
    m_refreshToken = m_settings.value("refreshToken").toString();
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
}

void BookOrbitClient::setBookQuery(const QString &sortField, const QStringList &formats)
{
    m_sortField = sortField;
    m_formats = formats;
    refreshBooks();
}

void BookOrbitClient::refreshBooks()
{
    m_queryGeneration++;
    m_loadedPages.clear();
    m_loadingPages.clear();
    const int generation = m_queryGeneration;
    loadLibrary([this, generation]() {
        if (generation != m_queryGeneration)
            return;
        // The A-Z sorts need their sections first: they say which rows start one
        if (letterSections()) {
            loadJumpSections([this, generation]() {
                if (generation == m_queryGeneration)
                    loadBooksPage(0);
            });
        } else {
            m_sectionStarts.clear();
            m_jumpSections.clear();
            emit jumpSectionsChanged();
            loadBooksPage(0);
        }
    });
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
    // Recently read and date added put the newest first; the A-Z sorts go ascending
    const bool newestFirst = m_sortField == "addedAt" || m_sortField == "lastReadAt";
    QJsonObject query{
        {"sort", QJsonArray{QJsonObject{{"field", m_sortField}, {"dir", newestFirst ? "desc" : "asc"}}}},
    };
    if (!m_formats.isEmpty()) {
        query.insert("filter", QJsonObject{
            {"type", "group"},
            {"join", "AND"},
            {"rules", QJsonArray{QJsonObject{
                {"type", "rule"},
                {"field", "format"},
                {"operator", "includesAny"},
                {"value", QJsonArray::fromStringList(m_formats)},
            }}},
        });
    }
    return query;
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
    // Pages of a new query wait for its first page, which sizes the list
    if (m_listGeneration != m_queryGeneration)
        return;
    first = qMax(0, first);
    last = qMin(last, m_books.count() - 1);
    for (int page = first / BOOKS_PAGE_SIZE; page <= last / BOOKS_PAGE_SIZE && last >= 0; page++) {
        if (!m_loadedPages.contains(page) && !m_loadingPages.contains(page))
            loadBooksPage(page);
    }
}

void BookOrbitClient::loadBooksPage(int page)
{
    // Same query the web client sends: returns {items: [...], total, page, size}. Pages
    // count from 0, as in the web client: page 1 is the second page.
    QJsonObject query = bookQuery();
    query.insert("pagination", QJsonObject{{"page", page}, {"size", BOOKS_PAGE_SIZE}});

    m_loadingPages.insert(page);
    const int generation = m_queryGeneration;
    callJson("POST", QString("/libraries/%1/books").arg(m_libraryId), QJsonDocument(query),
        [this, page, generation](const QJsonDocument &doc) {
            if (generation != m_queryGeneration)
                return;   // the sort or filter changed since this was asked for
            m_loadingPages.remove(page);
            m_loadedPages.insert(page);
            const QJsonObject result = doc.object();
            // The first page of a new query replaces the list with one of the right length
            if (m_listGeneration != generation) {
                m_listGeneration = generation;
                m_totalBooks = result.value("total").toInt();
                m_books.reset(m_totalBooks);
            }
            QVariantList books;
            for (const QJsonValue &item : result.value("items").toArray())
                books.append(decorateBook(item.toObject().toVariantMap()));
            const int first = page * BOOKS_PAGE_SIZE;
            m_books.setBooks(first, books);
            // Also the row after the page, whose section start depends on the last one here
            updateSectionStarts(first, first + int(books.size()));
            emit booksChanged();
        },
        [this, page, generation]() {
            if (generation == m_queryGeneration)
                m_loadingPages.remove(page);   // try again when it is next on screen
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

    // TODO: real on-device detection. Until then, a fixed random answer per book (the
    // same every run) so the row layout can be developed.
    row.insert("onDevice", qHash(book.value("id").toInt()) % 3 != 0);
    return row;
}

void BookOrbitClient::updateSectionStarts(int first, int last)
{
    for (int row = qMax(0, first); row <= last && row < m_books.count(); row++) {
        if (!m_books.isLoaded(row))
            continue;
        if (letterSections()) {
            m_books.setField(row, "sectionStart", m_sectionStarts.contains(row));
            m_books.setField(row, "sectionLabel", m_sectionStarts.value(row));
        } else {
            // A new day, if the row before is known to be a different one. Until that row
            // loads, no box: its page fills this in.
            const QString section = m_books.book(row).value("section").toString();
            m_books.setField(row, "sectionStart", row == 0
                || (m_books.isLoaded(row - 1) && m_books.book(row - 1).value("section").toString() != section));
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

void BookOrbitClient::send(const QByteArray &method, const QString &path, const QJsonDocument &body,
                           DataHandler onSuccess, ErrorHandler onError, bool withToken, bool quiet)
{
#ifndef EMULATOR
    // On the device, ask the system to bring up Wi-Fi first (shows its own dialog)
    if (!(QueryNetwork() & NET_CONNECTED)) {
        QElapsedTimer netTimer;
        netTimer.start();
        const int rc = NetConnect2(nullptr, 1);
        diagnostics::log(QString("NetConnect2 -> %1 after %2 ms").arg(rc).arg(netTimer.elapsed()));
    }
#endif

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
    m_books.reset(0);
    m_totalBooks = 0;
    m_libraryId = -1;
    m_loadedPages.clear();
    m_loadingPages.clear();
    m_queryGeneration++;
    m_listGeneration = -1;
    m_sectionStarts.clear();
    m_jumpSections.clear();
    emit jumpSectionsChanged();
    emit booksChanged();
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
