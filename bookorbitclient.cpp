#include "bookorbitclient.h"
#include "diagnostics.h"

#include <inkview.h>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QUrl>

// TODO: make the server configurable
static const char *DEFAULT_SERVER_URL = "https://books.example.com";

BookOrbitClient::BookOrbitClient(const QString &settingsPath, QObject *parent)
    : QObject(parent)
    , m_settings(settingsPath, QSettings::IniFormat)
    , m_serverUrl(QString::fromUtf8(DEFAULT_SERVER_URL))
{
    m_nam.setTransferTimeout(30000);
    m_username = m_settings.value("username").toString();
    m_refreshToken = m_settings.value("refreshToken").toString();
}

void BookOrbitClient::start()
{
    if (m_refreshToken.isEmpty())
        return;
    refreshSession([this]() {
        if (m_signedIn)
            refreshBooks();
    });
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
        [this](const QJsonDocument &doc) {
            applyCredentials(doc.object());
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
             [](const QJsonDocument &) {}, [](int, const QString &) {});
    }
    clearSession();
    setError(QString());
}

void BookOrbitClient::refreshBooks()
{
    // Same query the web client sends: returns {items: [...], total, page, size}
    QJsonObject query{
        {"sort", QJsonArray{QJsonObject{{"field", "addedAt"}, {"dir", "desc"}}}},
        {"pagination", QJsonObject{{"page", 1}, {"size", 20}}},
    };
    call("POST", "/books/query", QJsonDocument(query), [this](const QJsonDocument &doc) {
        const QJsonObject page = doc.object();
        m_books = page.value("items").toArray().toVariantList();
        m_totalBooks = page.value("total").toInt();
        emit booksChanged();
    });
}

void BookOrbitClient::call(const QByteArray &method, const QString &path, const QJsonDocument &body,
                           JsonHandler onSuccess, bool retried)
{
    if (m_refreshing) {
        m_afterRefresh.append([=]() {
            if (m_signedIn)
                call(method, path, body, onSuccess, retried);
        });
        return;
    }
    if (m_accessToken.isEmpty()) {
        setError("Not signed in");
        return;
    }
    send(method, path, body, onSuccess, [=](int status, const QString &message) {
        if (status == 401 && !retried) {
            // Access tokens last about 15 minutes: get a new one and try once more
            refreshSession([=]() {
                if (m_signedIn)
                    call(method, path, body, onSuccess, true);
            });
            return;
        }
        setError(message);
    }, true);
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
        [this, finish](const QJsonDocument &doc) {
            applyCredentials(doc.object());
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
                           JsonHandler onSuccess, ErrorHandler onError, bool withToken)
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
    request.setRawHeader("Accept", "application/json");
    if (withToken)
        request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    if (!body.isNull())
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam.sendCustomRequest(request, method, body.isNull() ? QByteArray() : body.toJson(QJsonDocument::Compact));
    m_pending++;
    emit busyChanged();
    QElapsedTimer timer;
    timer.start();

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        // Clear busy only once the result (or error) has been applied
        const auto done = qScopeGuard([this] { m_pending--; emit busyChanged(); });

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

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            onError(status, "Unexpected response from server: " + parseError.errorString());
            return;
        }
        onSuccess(doc);
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
