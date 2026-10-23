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

// Books per request to /books/query; more pages load as the list scrolls
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
    m_loadingPage = false;
    // Author sections need the names the server sorts by, which the book list doesn't have
    if (m_sortField == "author") {
        const int generation = m_queryGeneration;
        m_authorSortNames.clear();
        loadAuthorSortNames(0, [this, generation]() {
            if (generation == m_queryGeneration)
                loadBooksPage(0);
        });
        return;
    }
    loadBooksPage(0);
}

void BookOrbitClient::loadAuthorSortNames(int page, std::function<void()> then)
{
    // {items: [{name, sortName, ...}], total, page, size}; at most 100 a page, from page 0
    callJson("GET", QString("/authors?page=%1&size=100").arg(page), QJsonDocument(),
        [this, page, then](const QJsonDocument &doc) {
            const QJsonObject result = doc.object();
            for (const QJsonValue &author : result.value("items").toArray()) {
                const QString name = author["name"].toString();
                const QString sortName = author["sortName"].toString();
                m_authorSortNames.insert(name, sortName.isEmpty() ? name : sortName);
            }
            if (m_authorSortNames.size() < result.value("total").toInt() && !result.value("items").toArray().isEmpty())
                loadAuthorSortNames(page + 1, then);
            else
                then();
        });
}

void BookOrbitClient::loadMoreBooks()
{
    if (!m_loadingPage && hasMoreBooks())
        loadBooksPage(m_loadedPage + 1);
}

void BookOrbitClient::loadBooksPage(int page)
{
    // Same query the web client sends: returns {items: [...], total, page, size}. Pages
    // count from 0, as in the web client: page 1 is the second page.
    // Recently read and date added put the newest first; the A-Z sorts go ascending.
    const bool newestFirst = m_sortField == "addedAt" || m_sortField == "lastReadAt";
    QJsonObject query{
        {"sort", QJsonArray{QJsonObject{{"field", m_sortField}, {"dir", newestFirst ? "desc" : "asc"}}}},
        {"pagination", QJsonObject{{"page", page}, {"size", BOOKS_PAGE_SIZE}}},
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

    m_loadingPage = true;
    const int generation = m_queryGeneration;
    callJson("POST", "/books/query", QJsonDocument(query), [this, page, generation](const QJsonDocument &doc) {
        if (generation != m_queryGeneration)
            return;   // the sort or filter changed since this was asked for
        m_loadingPage = false;
        const QJsonObject result = doc.object();
        if (page == 0)
            m_books.clear();
        QVariantList books;
        // Sections are never empty, so the first book always starts one
        QString previousSection = m_books.last().value("section").toString();
        for (const QJsonValue &item : result.value("items").toArray()) {
            QVariantMap book = decorateBook(item.toObject().toVariantMap());
            // First row of a section: carries the letter or date box
            const QString section = book.value("section").toString();
            book.insert("sectionStart", section != previousSection);
            previousSection = section;
            books.append(book);
        }
        m_books.append(books);
        m_totalBooks = result.value("total").toInt();
        m_loadedPage = page;
        emit booksChanged();
    });
}

QVariantMap BookOrbitClient::decorateBook(const QVariantMap &book) const
{
    QVariantMap row = book;

    // Section: the first letter for the A-Z sorts (# for anything else), the day for the
    // date sorts. Author sort goes by the first author's sort name (see below).
    QString section;
    if (m_sortField == "title" || m_sortField == "author") {
        QString key = book.value("title").toString();
        if (m_sortField == "author") {
            // The server sorts by the first author's sort name ("Clark, Andy"), or the name
            // itself when there isn't one ("Cixin Liu")
            const QStringList authors = book.value("authors").toStringList();
            key = authors.isEmpty() ? QString() : m_authorSortNames.value(authors.first(), authors.first());
        }
        const QChar first = key.isEmpty() ? QChar() : key.at(0).toUpper();
        section = first.isLetter() && first.unicode() < 128 ? QString(first) : "#";
        row.insert("sectionLabel", section);
    } else {
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

void BookOrbitClient::download(const QString &path, DataHandler onSuccess, ErrorHandler onError)
{
    call("GET", path, QJsonDocument(), onSuccess, onError, true);
}

void BookOrbitClient::callJson(const QByteArray &method, const QString &path, const QJsonDocument &body,
                               std::function<void(const QJsonDocument &)> onSuccess)
{
    call(method, path, body,
        [this, onSuccess](const QByteArray &data) {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                setError("Unexpected response from server: " + parseError.errorString());
                return;
            }
            onSuccess(doc);
        },
        [this](int, const QString &message) {
            m_loadingPage = false;
            setError(message);
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
    m_books.clear();
    m_totalBooks = 0;
    m_loadedPage = -1;
    m_queryGeneration++;
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
