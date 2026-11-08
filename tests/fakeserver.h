#pragma once

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>

#include <functional>
#include <memory>

// HTTP server on 127.0.0.1 that records requests and answers from tables set by each test.
// A test holds replies and releases them in the order it wants, so nothing depends on time.
class FakeServer : public QObject
{
public:
    struct Request {
        QByteArray method;
        QString path;     // without /api/v1, with the query string
        QJsonObject body;
        QByteArray token;
    };
    struct Reply {
        int status = 200;
        QByteArray body = "{}";
    };
    using Predicate = std::function<bool(const Request &)>;

    explicit FakeServer(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection())
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { read(socket); });
        });
        m_server.listen(QHostAddress::LocalHost);
        m_port = m_server.serverPort();
    }

    QString url() const { return QString("http://127.0.0.1:%1").arg(m_port); }

    // Answers method + path (query string ignored) from now on
    void route(const QByteArray &method, const QString &path, int status, const QByteArray &body)
    {
        m_routes.insert(method + ' ' + path, Reply{status, body});
    }
    void route(const QByteArray &method, const QString &path, const QJsonDocument &body)
    {
        route(method, path, 200, body.toJson(QJsonDocument::Compact));
    }
    void setLibraries(const QList<QPair<int, QString>> &libraries)
    {
        QJsonArray array;
        for (const auto &library : libraries)
            array.append(QJsonObject{{"id", library.first}, {"name", library.second}, {"bookCount", 2}});
        route("GET", "/libraries", QJsonDocument(array));
    }
    // A library with books 'id * 100 + n', titled "L<id> book <n>"
    void addLibrary(int id, int books = 2)
    {
        QJsonArray items;
        for (int n = 1; n <= books; n++) {
            items.append(QJsonObject{{"id", id * 100 + n},
                                     {"title", QString("L%1 book %2").arg(id).arg(n)},
                                     {"authors", QJsonArray{QString("Author L%1").arg(id)}}});
        }
        const QString path = QString("/libraries/%1/books").arg(id);
        route("POST", path, QJsonDocument(QJsonObject{{"items", items}, {"total", books}, {"page", 0}, {"size", 40}}));
        route("POST", path + "/jump-buckets", QJsonDocument(QJsonObject{{"kind", "letter"},
            {"buckets", QJsonArray{QJsonObject{{"key", "l"}, {"label", QString("L%1").arg(id)}, {"index", 0}}}}}));
    }
    // Series and authors of every library, answered by the libraryId in the query
    void routeGroups() { m_groups = true; }

    // Replies to matching requests wait for release(), until clearHolds()
    void hold(Predicate predicate) { m_holds.append(predicate); }
    void holdPath(const QString &prefix)
    {
        hold([prefix](const Request &request) { return request.path.startsWith(prefix); });
    }
    // Holds only the first matching request
    void holdOnce(Predicate predicate)
    {
        auto left = std::make_shared<int>(1);
        hold([predicate, left](const Request &request) {
            if (*left > 0 && predicate(request)) {
                --*left;
                return true;
            }
            return false;
        });
    }
    void clearHolds() { m_holds.clear(); }
    // Stops answering, as though the server had gone, and comes back on the same port
    void stop()
    {
        m_server.close();
        for (QTcpSocket *socket : m_server.findChildren<QTcpSocket *>())
            socket->abort();
    }
    void resume() { m_server.listen(QHostAddress::LocalHost, m_port); }
    int heldCount() const { return int(m_held.size()); }
    int heldCount(const QString &prefix) const
    {
        int count = 0;
        for (const Held &held : m_held) {
            if (held.request.path.startsWith(prefix))
                count++;
        }
        return count;
    }
    // Sends the first held reply for a path with this prefix, as it was going to be or as
    // given
    bool release(const QString &prefix = QString(), const Reply *instead = nullptr)
    {
        for (int i = 0; i < m_held.size(); i++) {
            if (!m_held.at(i).request.path.startsWith(prefix))
                continue;
            const Held held = m_held.takeAt(i);
            answer(held.socket, instead ? *instead : held.reply);
            return true;
        }
        return false;
    }
    void releaseAll(const QString &prefix = QString())
    {
        while (release(prefix)) {}
    }

    QList<Request> requests;
    void clearRequests() { requests.clear(); }
    int count(const QByteArray &method, const QString &prefix) const
    {
        int found = 0;
        for (const Request &request : requests) {
            if (request.method == method && request.path.startsWith(prefix))
                found++;
        }
        return found;
    }
    int countExact(const QByteArray &method, const QString &path) const
    {
        int found = 0;
        for (const Request &request : requests) {
            if (request.method == method && request.path == path)
                found++;
        }
        return found;
    }
    int countContaining(const QString &text) const
    {
        int found = 0;
        for (const Request &request : requests) {
            if (request.path.contains(text))
                found++;
        }
        return found;
    }

private:
    struct Held {
        QPointer<QTcpSocket> socket;
        Request request;
        Reply reply;
    };

    void read(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        for (;;) {
            const int headerEnd = int(buffer.indexOf("\r\n\r\n"));
            if (headerEnd < 0)
                return;
            const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
            int length = 0;
            QByteArray token;
            for (const QByteArray &line : lines) {
                const QByteArray lower = line.toLower();
                if (lower.startsWith("content-length:"))
                    length = line.mid(15).trimmed().toInt();
                if (lower.startsWith("authorization: bearer "))
                    token = line.mid(22).trimmed();
            }
            if (buffer.size() < headerEnd + 4 + length)
                return;
            const QList<QByteArray> first = lines.first().trimmed().split(' ');
            Request request;
            request.method = first.value(0);
            request.path = QString::fromUtf8(first.value(1));
            if (request.path.startsWith("/api/v1"))
                request.path = request.path.mid(7);
            request.body = QJsonDocument::fromJson(buffer.mid(headerEnd + 4, length)).object();
            request.token = token;
            buffer.remove(0, headerEnd + 4 + length);
            handle(socket, request);
        }
    }

    Reply replyFor(const Request &request) const
    {
        const QString bare = request.path.section('?', 0, 0);
        if (m_groups && request.method == "GET" && (bare == "/series" || bare == "/authors")) {
            const QString query = request.path.section('?', 1);
            QString library;
            for (const QString &part : query.split('&')) {
                if (part.startsWith("libraryId="))
                    library = part.mid(10);
            }
            const QString name = bare == "/series" ? QString("Series L%1").arg(library)
                                                   : QString("Author L%1").arg(library);
            const QJsonObject row{{"id", library.toInt() * 10}, {"name", name}, {"bookCount", 1}};
            return Reply{200, QJsonDocument(QJsonObject{{"items", QJsonArray{row}}, {"total", 1}})
                                  .toJson(QJsonDocument::Compact)};
        }
        const auto it = m_routes.constFind(request.method + ' ' + bare);
        if (it != m_routes.constEnd())
            return it.value();
        return Reply{404, R"({"statusCode":404,"message":"Not found"})"};
    }

    void handle(QTcpSocket *socket, const Request &request)
    {
        requests.append(request);
        const Reply reply = replyFor(request);
        for (const Predicate &predicate : std::as_const(m_holds)) {
            if (predicate(request)) {
                m_held.append(Held{socket, request, reply});
                return;
            }
        }
        answer(socket, reply);
    }

    static void answer(QTcpSocket *socket, const Reply &reply)
    {
        if (!socket)
            return;
        QByteArray text = "HTTP/1.1 " + QByteArray::number(reply.status) + " Status\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: " + QByteArray::number(reply.body.size()) + "\r\n\r\n";
        socket->write(text + reply.body);
        socket->flush();
    }

    QTcpServer m_server;
    QHash<QString, Reply> m_routes;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<Predicate> m_holds;
    QList<Held> m_held;
    bool m_groups = false;
    quint16 m_port = 0;
};
