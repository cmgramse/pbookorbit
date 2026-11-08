#include "fakeserver.h"

#include "core/bookfinder.h"
#include "core/bookorbitclient.h"
#include "core/database.h"
#include "core/downloads.h"
#include "core/libraryselection.h"

#include <inkview.h>

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using Request = FakeServer::Request;
using Reply = FakeServer::Reply;

static const QString USER = "Reader";

// Time for a released reply to reach the client. Only used before asserting that it changed
// nothing; replies are ordered by releasing them, never by waiting.
static void settle()
{
    QTest::qWait(150);
}

static bool isPage(const Request &request, int library)
{
    return request.path == QString("/libraries/%1/books").arg(library) && !request.body.contains("filter");
}
static bool isCompanionLookup(const Request &request, int library)
{
    return request.path == QString("/libraries/%1/books").arg(library)
           && request.body.value("filter").toObject().value("join").toString() == "OR";
}
static bool isGroupBooks(const Request &request, int library)
{
    return request.path == QString("/libraries/%1/books").arg(library)
           && request.body.value("filter").toObject().value("join").toString() == "AND";
}

static QStringList titles(BookListModel *model)
{
    QStringList found;
    for (int i = 0; i < model->count(); i++) {
        if (model->isLoaded(i))
            found.append(model->book(i).value("title").toString());
    }
    return found;
}
static QStringList names(BookListModel *model)
{
    QStringList found;
    for (int i = 0; i < model->count(); i++)
        found.append(model->book(i).value("name").toString());
    return found;
}
static QVariantMap localBook(int id, const QString &title)
{
    return QVariantMap{{"id", id}, {"title", title}, {"authors", QStringList{"Local Author"}}};
}

// A genre list page request: the library's books with any genre
static bool isGenreList(const Request &request, int library)
{
    if (request.path != QString("/libraries/%1/books").arg(library))
        return false;
    const QJsonObject first = request.body.value("filter").toObject().value("rules").toArray().at(0).toObject();
    return first.value("field").toString() == "genre" && first.value("operator").toString() == "isNotEmpty";
}
static int pageOf(const Request &request)
{
    return request.body.value("pagination").toObject().value("page").toInt();
}
// A book list item with genres (any JSON value, as the server might send) and one file
static QJsonObject genreBook(int id, const QString &title, const QJsonValue &genres,
                             const QString &format = "epub", const QString &author = "Genre Author")
{
    QJsonObject book{{"id", id}, {"title", title}, {"authors", QJsonArray{author}},
                     {"files", QJsonArray{QJsonObject{{"id", id * 10}, {"role", "primary"}, {"format", format}}}}};
    if (!genres.isUndefined())
        book.insert("genres", genres);
    return book;
}
static Reply genrePage(const QJsonArray &items, int total)
{
    return Reply{200, QJsonDocument(QJsonObject{{"items", items}, {"total", total}}).toJson(QJsonDocument::Compact)};
}
static QVariantMap rowNamed(BookListModel *model, const QString &name)
{
    for (int i = 0; i < model->count(); i++) {
        if (model->book(i).value("name").toString() == name)
            return model->book(i);
    }
    return QVariantMap();
}
static QStringList cachedNames(const QVariantMap &cache)
{
    QStringList found;
    for (const QVariant &row : cache.value("rows").toList())
        found.append(row.toMap().value("name").toString());
    return found;
}

// An unrecorded book for BookFinder to look for, in the stub's scratch folder
struct FindFile {
    FindFile()
    {
        QDir().mkpath(FLASHDIR "/Books");
        QFile file(FLASHDIR "/Books/Synthetic title.epub");
        file.open(QIODevice::WriteOnly);
        file.write("synthetic");
    }
    ~FindFile() { QDir(FLASHDIR "/Books").removeRecursively(); }
};

class LibraryTest : public QObject
{
    Q_OBJECT

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<FakeServer> m_server;
    QScopedPointer<Database> m_database;
    QScopedPointer<BookOrbitClient> m_client;

    FakeServer &server() { return *m_server; }
    Database &database() { return *m_database; }
    BookOrbitClient &client() { return *m_client; }

    QString key(const QString &user = USER) const { return libraryPreferenceKey(m_server->url(), user); }

    void routeAuth(FakeServer &target)
    {
        const QJsonDocument credentials(QJsonObject{{"accessToken", "access"}, {"refreshToken", "refresh"}});
        target.route("POST", "/auth/login", credentials);
        target.route("POST", "/auth/refresh", credentials);
        target.route("POST", "/auth/logout", QJsonDocument(QJsonObject{}));
        target.route("GET", "/user-preferences/theme", QJsonDocument(QJsonObject{}));
        target.routeGroups();
    }
    void twoLibraries()
    {
        server().setLibraries({{1, "Alpha"}, {2, "Beta"}});
        server().addLibrary(1);
        server().addLibrary(2);
    }
    void newClient() { m_client.reset(new BookOrbitClient(m_database.data())); }
    // Answers library's genre list pages from pages, by page number; later pages are empty
    void routeGenres(int library, const QList<Reply> &pages)
    {
        server().routeWhen([library](const Request &r) { return isGenreList(r, library); },
            [pages](const Request &r) { return pages.value(pageOf(r), genrePage({}, 0)); });
    }
    bool genresSettled()
    {
        return client().genresStatus() != "loading" && client().genresStatus() != "idle";
    }
    // A session saved by an earlier run, with the client constructed but not started
    void savedSession()
    {
        database().setText("serverUrl", server().url());
        database().setText("username", USER);
        database().setText("refreshToken", "saved");
        newClient();
    }
    bool signInAndLoad(const QString &user = USER)
    {
        client().signIn(server().url(), user, "secret");
        return QTest::qWaitFor([this] {
            return client().signedIn() && client().currentLibraryId() > 0
                   && client().books()->count() > 0 && (!client().busy() || server().heldCount() > 0);
        }, 5000);
    }

private slots:
    void init()
    {
        QDir(FLASHDIR).removeRecursively();
        QDir().mkpath(FLASHDIR);
        m_dir.reset(new QTemporaryDir);
        m_server.reset(new FakeServer);
        m_database.reset(new Database(m_dir->filePath("test.db")));
        routeAuth(server());
        newClient();
    }
    void cleanup()
    {
        m_client.reset();
        m_database.reset();
        m_server.reset();
        m_dir.reset();
        qunsetenv("PBOOKORBIT_OFFLINE");
    }

    // T1
    void parseKeepsOrderAndDropsBadEntries()
    {
        const QJsonDocument doc = QJsonDocument::fromJson("[ {\"id\": 7, \"name\": \"Seven\", \"bookCount\": 3}, {\"id\": 0, \"name\": \"Zero\"}, {\"id\": -2, \"name\": \"Neg\"}, {\"id\": \"x\", \"name\": \"Text\"}, {\"name\": \"None\"}, {\"id\": 2, \"name\": \"Two\"}, {\"id\": 7, \"name\": \"Again\"}]");
        QList<LibraryChoice> libraries;
        QVERIFY(parseLibraries(doc, &libraries));
        QCOMPARE(libraries.size(), 2);
        QCOMPARE(libraries.at(0).id, 7);
        QCOMPARE(libraries.at(0).bookCount, 3);
        QCOMPARE(libraries.at(1).id, 2);
    }
    void parseTellsMalformedFromEmpty()
    {
        QList<LibraryChoice> libraries;
        QVERIFY(parseLibraries(QJsonDocument::fromJson("[]"), &libraries));
        QVERIFY(libraries.isEmpty());
        QVERIFY(!parseLibraries(QJsonDocument::fromJson("{\"id\": 1}"), &libraries));
        QVERIFY(!parseLibraries(QJsonDocument(), &libraries));
        QVERIFY(!parseLibraries(QJsonDocument::fromJson("[{\"id\": 0}]"), &libraries));
    }
    void parseLabelsEmptyAndDuplicateNames()
    {
        QList<LibraryChoice> libraries;
        QVERIFY(parseLibraries(QJsonDocument::fromJson(
            "[{\"id\": 1, \"name\": \"\"}, {\"id\": 2, \"name\": \"Same\"}, {\"id\": 3, \"name\": \"Same\"}, {\"id\": 4, \"name\": \"Solo\"}]"), &libraries));
        QCOMPARE(libraries.at(0).label, QString("Library 1"));
        QCOMPARE(libraries.at(1).label, QString("Same #2"));
        QCOMPARE(libraries.at(2).label, QString("Same #3"));
        QCOMPARE(libraries.at(3).label, QString("Solo"));
    }
    void chooseSavedElseFirstElseNone()
    {
        const QList<LibraryChoice> libraries{{5, "A", "A", 0}, {9, "B", "B", 0}};
        QCOMPARE(chooseLibrary(libraries, 9), 9);
        QCOMPARE(chooseLibrary(libraries, 3), 5);
        QCOMPARE(chooseLibrary(libraries, 0), 5);
        QCOMPARE(chooseLibrary({}, 9), -1);
    }
    // T2
    void preferenceKeyFoldsCaseAndSeparatesScopes()
    {
        QCOMPARE(libraryPreferenceKey("https://a.example", "Reader"), QString("selectedLibrary|https://a.example|reader"));
        QCOMPARE(libraryPreferenceKey("https://a.example", "READER"), libraryPreferenceKey("https://a.example", "reader"));
        QVERIFY(libraryPreferenceKey("https://a.example", "reader") != libraryPreferenceKey("https://b.example", "reader"));
        QVERIFY(libraryPreferenceKey("https://a.example", "one") != libraryPreferenceKey("https://a.example", "two"));
    }

    // T3
    void firstResolveWithNoLibraries()
    {
        server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));
        client().signIn(server().url(), USER, "secret");
        QTRY_COMPARE(client().error(), QString("No libraries available to this account"));
        QCOMPARE(client().currentLibraryId(), -1);
        QVERIFY(client().libraries().isEmpty());
        QCOMPARE(server().countContaining("/libraries/-1"), 0);
    }
    void firstResolveWithOneLibrary()
    {
        server().setLibraries({{4, "Only"}});
        server().addLibrary(4);
        QVERIFY(signInAndLoad());
        QCOMPARE(client().currentLibraryId(), 4);
        QCOMPARE(client().currentLibraryLabel(), QString("Only"));
        QCOMPARE(client().libraries().size(), 1);
        QCOMPARE(titles(client().books()).value(0), QString("L4 book 1"));
    }
    void firstResolveWithTwoPersistsFirstInServerOrder()
    {
        server().setLibraries({{2, "Beta"}, {1, "Alpha"}});
        server().addLibrary(1);
        server().addLibrary(2);
        QSignalSpy fallback(&client(), &BookOrbitClient::libraryFallback);
        QSignalSpy switched(&client(), &BookOrbitClient::librarySwitched);
        QVERIFY(signInAndLoad());
        QCOMPARE(client().currentLibraryId(), 2);
        QCOMPARE(client().libraries().size(), 2);
        QCOMPARE(database().text(key()), QString("2"));
        QCOMPARE(fallback.count(), 0);
        QCOMPARE(switched.count(), 0);
        QCOMPARE(server().countExact("GET", "/libraries"), 1);
    }

    // T4
    void restartKeepsTheChoiceWithoutFirstLibraryRows()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QTRY_VERIFY_WITH_TIMEOUT(!database().listCache("library:2").isEmpty(), 5000);

        server().clearRequests();
        newClient();
        // B's cached rows show at once, while its first page is held
        server().holdPath("/libraries/2/books");
        client().start();
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QTRY_VERIFY(server().heldCount() > 0);
        QCOMPARE(client().currentLibraryId(), 2);
        server().clearHolds();
        server().releaseAll();
        QTRY_VERIFY(!client().busy());
        QTRY_COMPARE(titles(client().books()).size(), 2);
        QCOMPARE(server().countContaining("/libraries/1/"), 0);
    }

    // T5
    void accountsAndServersDoNotInheritAChoice()
    {
        twoLibraries();
        QVERIFY(signInAndLoad("one"));
        client().selectLibrary(2);
        QTRY_COMPARE(client().currentLibraryId(), 2);
        QCOMPARE(database().text(key("one")), QString("2"));

        client().signOut();
        QCOMPARE(database().text(key("one")), QString("2"));   // kept for coming back
        QCOMPARE(client().currentLibraryId(), -1);

        QVERIFY(signInAndLoad("two"));
        QCOMPARE(client().currentLibraryId(), 1);
        client().signOut();

        QVERIFY(signInAndLoad("ONE"));   // the server matches usernames ignoring case
        QCOMPARE(client().currentLibraryId(), 2);
        client().signOut();

        FakeServer other;
        routeAuth(other);
        other.setLibraries({{1, "Other one"}, {2, "Other two"}});
        other.addLibrary(1);
        other.addLibrary(2);
        client().signIn(other.url(), "one", "secret");
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QTRY_VERIFY(!client().busy());
    }

    // T6
    void fallbackFromSavedPersistsAndNotifiesOnce()
    {
        twoLibraries();
        savedSession();
        database().setText(key(), "9");
        QSignalSpy fallback(&client(), &BookOrbitClient::libraryFallback);
        client().start();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L1 book 1"));
        QCOMPARE(fallback.count(), 1);
        QCOMPARE(database().text(key()), QString("1"));
        QCOMPARE(server().countContaining("/libraries/9"), 0);
    }
    void fallbackDropsRowsCachedForTheGoneLibrary()
    {
        twoLibraries();
        savedSession();
        database().setText(key(), "9");
        database().saveListCache("series:9", QVariantMap{{"rows", QVariantList{QVariantMap{{"name", "Gone series"}}}}});
        server().holdPath("/libraries");
        client().start();
        client().loadSeries();
        QTRY_COMPARE(names(client().seriesList()), QStringList{"Gone series"});
        server().clearHolds();
        server().releaseAll();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QVERIFY(!names(client().seriesList()).contains("Gone series"));
        QTRY_COMPARE(names(client().seriesList()), QStringList{"Series L1"});
    }
    void savedLibraryStaysWhenOrderChanges()
    {
        server().setLibraries({{2, "Beta"}, {1, "Alpha"}});
        server().addLibrary(1);
        server().addLibrary(2);
        savedSession();
        database().setText(key(), "1");
        QSignalSpy fallback(&client(), &BookOrbitClient::libraryFallback);
        client().start();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QCOMPARE(fallback.count(), 0);
    }
    void emptyListKeepsThePreference()
    {
        server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));
        savedSession();
        database().setText(key(), "2");
        client().start();
        QTRY_COMPARE(client().error(), QString("No libraries available to this account"));
        QCOMPARE(database().text(key()), QString("2"));
        QCOMPARE(server().countContaining("/libraries/-1"), 0);
    }
    void malformedListChangesNothing()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QTRY_VERIFY(!client().busy());
        server().route("GET", "/libraries", 200, "{\"not\": \"a list\"}");
        client().refreshLibraries();
        QTRY_COMPARE(client().error(), QString("Unexpected response from server"));
        QCOMPARE(client().currentLibraryId(), 2);
        QCOMPARE(database().text(key()), QString("2"));
    }

    // The list reloads by itself when libraries come back
    void listsReloadWhenLibrariesComeBack()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().loadSeries();
        QTRY_VERIFY(client().seriesLoaded());
        server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));
        client().refreshLibraries();
        QTRY_COMPARE(client().currentLibraryId(), -1);
        QCOMPARE(client().books()->count(), 0);
        QVERIFY(!client().error().isEmpty());

        twoLibraries();
        QSignalSpy switched(&client(), &BookOrbitClient::librarySwitched);
        QSignalSpy fallback(&client(), &BookOrbitClient::libraryFallback);
        client().refreshLibraries();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L1 book 1"));
        QCOMPARE(client().error(), QString());
        QCOMPARE(switched.count(), 1);   // so the visible series or authors tab reloads
        QCOMPARE(fallback.count(), 0);
    }
    void listReloadsWhenAFailedFirstResolveIsRetried()
    {
        server().route("GET", "/libraries", 500, "{\"message\":\"Broken\"}");
        client().signIn(server().url(), USER, "secret");
        QTRY_VERIFY(client().error().contains("Broken"));
        twoLibraries();
        client().refreshLibraries();
        QTRY_COMPARE(titles(client().books()).value(0), QString("L1 book 1"));
        QCOMPARE(client().error(), QString());
    }
    void comingBackDoesNotDisturbALocalList()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().setLocalBooks({localBook(101, "Downloaded A")});
        client().setBookQuery("title", false, {}, true);
        server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));
        client().refreshLibraries();
        QTRY_COMPARE(client().currentLibraryId(), -1);
        twoLibraries();
        server().clearRequests();
        client().refreshLibraries();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QTRY_VERIFY(!client().busy());
        QCOMPARE(titles(client().books()), QStringList{"Downloaded A"});
        QCOMPARE(server().count("POST", "/libraries/1/books"), 0);
        QCOMPARE(client().error(), QString());
    }
    void aFirstResolveWithNothingWaitingEmitsNoSwitch()
    {
        twoLibraries();
        savedSession();
        client().setLocalBooks({localBook(101, "Downloaded A")});
        client().setBookQuery("title", false, {}, true);
        QSignalSpy switched(&client(), &BookOrbitClient::librarySwitched);
        client().start();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QCOMPARE(switched.count(), 0);
    }

    // Review finding AR3: local-only lists survive every resolution
    void localOnlyListSurvivesFallbackAtFirstResolve()
    {
        twoLibraries();
        savedSession();
        database().setText(key(), "9");
        client().setLocalBooks({localBook(101, "Downloaded A"), localBook(201, "Downloaded B")});
        client().setBookQuery("title", false, {}, true);
        client().start();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QTRY_VERIFY(!client().busy());
        QCOMPARE(titles(client().books()), (QStringList{"Downloaded A", "Downloaded B"}));
        QCOMPARE(server().countContaining("/libraries/-1"), 0);
    }
    void localOnlyListSurvivesEmptyAtFirstResolve()
    {
        server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));
        savedSession();
        database().setText(key(), "2");
        client().setLocalBooks({localBook(101, "Downloaded A")});
        client().setBookQuery("title", false, {}, true);
        client().start();
        QTRY_VERIFY(!client().error().isEmpty());
        QCOMPARE(titles(client().books()), QStringList{"Downloaded A"});
        QCOMPARE(server().countContaining("/libraries/-1"), 0);
    }
    void localOnlyListSurvivesEmptyMidSession()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().setLocalBooks({localBook(101, "Downloaded A")});
        client().setBookQuery("title", false, {}, true);
        QCOMPARE(titles(client().books()), QStringList{"Downloaded A"});
        server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));
        client().refreshLibraries();
        QTRY_VERIFY(!client().error().isEmpty());
        QCOMPARE(client().currentLibraryId(), -1);
        QCOMPARE(titles(client().books()), QStringList{"Downloaded A"});
        QCOMPARE(server().countContaining("/libraries/-1"), 0);
    }
    void localOnlyListSurvivesFallbackMidSession()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        client().setLocalBooks({localBook(101, "Downloaded A")});
        client().setBookQuery("title", false, {}, true);
        server().setLibraries({{1, "Alpha"}});
        QSignalSpy fallback(&client(), &BookOrbitClient::libraryFallback);
        client().refreshLibraries();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QCOMPARE(fallback.count(), 1);
        QCOMPARE(titles(client().books()), QStringList{"Downloaded A"});
    }

    void localOnlyListSurvivesFallbackAndEmptyOnReconnect_data()
    {
        QTest::addColumn<bool>("empty");
        QTest::addRow("saved missing") << false;
        QTest::addRow("empty") << true;
    }
    void localOnlyListSurvivesFallbackAndEmptyOnReconnect()
    {
        QFETCH(bool, empty);
        twoLibraries();
        if (empty)
            server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));
        savedSession();
        database().setText(key(), "9");
        client().setLocalBooks({localBook(101, "Downloaded A")});
        client().setBookQuery("title", false, {}, true);
        server().stop();
        client().start();
        QTRY_VERIFY(!client().online());
        server().resume();
        server().clearRequests();
        client().goOnline();
        QTRY_VERIFY(client().online());
        QTRY_VERIFY(server().countExact("GET", "/libraries") > 0);
        QTRY_VERIFY(!client().busy());
        settle();
        QCOMPARE(titles(client().books()), QStringList{"Downloaded A"});
        QCOMPARE(server().countExact("GET", "/libraries"), 1);
        QCOMPARE(server().countContaining("/libraries/-1"), 0);
    }

    // T7
    void forbiddenCurrentLibrarySwitchesIfUnlisted()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        server().route("POST", "/libraries/1/books/jump-buckets", 403, "{\"message\":\"Forbidden\"}");
        server().setLibraries({{2, "Beta"}});
        server().clearRequests();
        QSignalSpy fallback(&client(), &BookOrbitClient::libraryFallback);
        client().refreshBooks();
        QTRY_COMPARE(client().currentLibraryId(), 2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QCOMPARE(fallback.count(), 1);
        QCOMPARE(client().error(), QString());
        QCOMPARE(server().countExact("GET", "/libraries"), 1);
    }
    void forbiddenCurrentLibraryStillListedDoesNotLoop()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        server().route("POST", "/libraries/1/books/jump-buckets", 403, "{\"message\":\"Forbidden\"}");
        server().clearRequests();
        client().refreshBooks();
        QTRY_COMPARE(server().countExact("GET", "/libraries"), 1);
        QTRY_VERIFY(!client().busy());
        settle();
        QCOMPARE(server().countExact("GET", "/libraries"), 1);
        QCOMPARE(client().currentLibraryId(), 1);
        QVERIFY(client().error().contains("403"));
    }
    // Review finding AR4
    void staleForbiddenAfterComingBackIsIgnored()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        server().holdOnce([](const Request &request) { return request.path == "/libraries/1/books/jump-buckets"; });
        client().refreshBooks();
        QTRY_COMPARE(server().heldCount(), 1);
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        client().selectLibrary(1);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L1 book 1"));
        QTRY_VERIFY(!client().busy() || server().heldCount() == 1);
        server().clearRequests();
        const Reply forbidden{403, "{\"message\":\"Forbidden\"}"};
        QSignalSpy busy(&client(), &BookOrbitClient::busyChanged);
        QVERIFY(server().release("/libraries/1", &forbidden));
        QTRY_VERIFY(busy.count() > 0);   // the reply has arrived
        settle();
        QCOMPARE(server().countExact("GET", "/libraries"), 0);
        QCOMPARE(client().error(), QString());
        QCOMPARE(client().currentLibraryId(), 1);
        QCOMPARE(database().text(key()), QString("1"));
    }

    void currentForbiddenAfterComingBackRefetchesOnce()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        client().selectLibrary(1);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L1 book 1"));
        QTRY_VERIFY(!client().busy());
        server().route("POST", "/libraries/1/books/jump-buckets", 403, "{\"message\":\"Forbidden\"}");
        server().clearRequests();
        client().refreshBooks();
        QTRY_VERIFY(!client().error().isEmpty());
        QTRY_VERIFY(!client().busy());
        settle();
        QCOMPARE(server().countExact("GET", "/libraries"), 1);
    }
    void forbiddenFromTheOldAccountWithTheSameIdIsIgnored()
    {
        twoLibraries();
        QVERIFY(signInAndLoad("one"));
        server().holdOnce([](const Request &r) { return r.path == "/libraries/1/books/jump-buckets"; });
        client().refreshBooks();
        QTRY_COMPARE(server().heldCount(), 1);
        client().signOut();
        QVERIFY(signInAndLoad("two"));
        QCOMPARE(client().currentLibraryId(), 1);
        server().clearRequests();
        const Reply forbidden{403, "{\"message\":\"Forbidden\"}"};
        QVERIFY(server().release("/libraries/1", &forbidden));
        settle();
        QCOMPARE(server().countExact("GET", "/libraries"), 0);
        QCOMPARE(client().error(), QString());
        QCOMPARE(client().currentLibraryId(), 1);
    }

    // T8
    void switchResetsEveryViewAndReloadsTheNewLibrary()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().setBookQuery("addedAt", true, {"epub"}, false);
        QTRY_VERIFY(!client().busy());
        client().loadSeries();
        client().loadAuthors();
        client().showAuthorSeries("Author L1");
        client().showBooksOf("author", {"Author L1"}, "title", false);
        client().search("book");
        QTRY_VERIFY(client().seriesLoaded() && client().authorsLoaded() && client().authorSeriesLoaded());
        QTRY_VERIFY(client().booksOf()->count() > 0 && client().searchResults()->count() > 0);
        QTRY_VERIFY(!client().busy());
        server().clearRequests();

        bool checked = false;
        connect(&client(), &BookOrbitClient::librarySwitched, this, [&] {
            checked = true;
            QCOMPARE(client().currentLibraryId(), 2);
            QCOMPARE(client().books()->count(), 0);
            QCOMPARE(client().searchResults()->count(), 0);
            QCOMPARE(client().booksOf()->count(), 0);
            QCOMPARE(client().seriesList()->count(), 0);
            QCOMPARE(client().authorsList()->count(), 0);
            QCOMPARE(client().authorSeriesList()->count(), 0);
            QVERIFY(!client().seriesLoaded());
            QVERIFY(!client().authorsLoaded());
            QVERIFY(!client().authorBooksLoaded());
            QVERIFY(!client().authorSeriesLoaded());
            QVERIFY(client().jumpSections().isEmpty());
            QVERIFY(client().seriesJumpSections().isEmpty());
        });
        QSignalSpy changed(&client(), &BookOrbitClient::currentLibraryChanged);
        client().selectLibrary(2);
        QVERIFY(checked);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(client().currentLibraryLabel(), QString("Beta"));
        QCOMPARE(database().text(key()), QString("2"));

        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        // Sort and filter kept
        QJsonObject query;
        for (const Request &request : std::as_const(server().requests)) {
            if (request.path == "/libraries/2/books" && request.body.contains("sort"))
                query = request.body;
        }
        QCOMPARE(query.value("sort").toArray().at(0).toObject().value("field").toString(), QString("addedAt"));
        QVERIFY(QJsonDocument(query).toJson().contains("epub"));
        // Closed views are invalidated, not reloaded
        QCOMPARE(server().count("GET", "/series"), 0);
        QCOMPARE(server().count("GET", "/authors"), 0);
        QCOMPARE(server().countContaining("/libraries/1/"), 0);

        client().loadSeries();
        client().loadAuthors();
        QTRY_COMPARE(names(client().seriesList()), QStringList{"Series L2"});
        QTRY_COMPARE(names(client().authorsList()), QStringList{"Author L2"});
        client().search("book");
        QTRY_COMPARE(titles(client().searchResults()).value(0), QString("L2 book 1"));
    }

    // T9
    void cachesDoNotCrossLibraries()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().loadSeries();
        QTRY_VERIFY(client().seriesLoaded());
        QTRY_VERIFY_WITH_TIMEOUT(!database().listCache("series:1").isEmpty(), 5000);
        database().saveListCache("series:2", QVariantMap{{"rows", QVariantList{QVariantMap{{"name", "Cached B"}}}}});
        // Library 1's cache timer is running from its first page, and mustn't save under 2
        client().refreshBooks();
        QTRY_VERIFY(!client().busy());
        server().holdPath("/libraries/2/books");
        client().selectLibrary(2);
        server().holdPath("/series");
        client().loadSeries();
        QCOMPARE(names(client().seriesList()), QStringList{"Cached B"});
        QTest::qWait(2200);   // past the cache timer's interval, to see it stopped
        QVERIFY(database().listCache("library:2").isEmpty());
        QVERIFY(database().listCache("library").isEmpty());
        server().clearHolds();
        server().releaseAll();
        QTRY_VERIFY(!client().busy());
    }
    void oldUnscopedCacheIsNotRead()
    {
        twoLibraries();
        database().saveListCache("series", QVariantMap{{"rows", QVariantList{QVariantMap{{"name", "Old unscoped"}}}}});
        savedSession();
        server().holdPath("/libraries");
        client().start();
        client().loadSeries();
        QVERIFY(!names(client().seriesList()).contains("Old unscoped"));
        server().clearHolds();
        server().releaseAll();
        QTRY_COMPARE(names(client().seriesList()), QStringList{"Series L1"});
    }

    // T10, T11: a held reply for library 1, released after switching to 2
    void lateReplies_data()
    {
        QTest::addColumn<QString>("family");
        QTest::addColumn<int>("status");
        for (const char *family : {"page", "jump", "seriesNumbers", "companions", "series", "authors",
                                   "seriesBooks", "authorBooks", "authorSeries"}) {
            QTest::addRow("%s success", family) << QString(family) << 200;
            QTest::addRow("%s failure", family) << QString(family) << 500;
        }
    }
    void lateReplies()
    {
        QFETCH(QString, family);
        QFETCH(int, status);
        twoLibraries();
        QVERIFY(signInAndLoad());
        server().clearRequests();
        QString heldPath = "/libraries/1/books";
        if (family == "page") {
            server().holdOnce([](const Request &r) { return isPage(r, 1); });
            client().setBookQuery("addedAt", true, {});
        } else if (family == "jump") {
            server().holdOnce([](const Request &r) { return r.path == "/libraries/1/books/jump-buckets"; });
            client().refreshBooks();
        } else if (family == "seriesNumbers") {
            server().holdOnce([](const Request &r) {
                return r.path == "/libraries/1/books" && r.body.value("pagination").toObject().value("size").toInt() == 200;
            });
            client().showBooksOf("series", {"Series L1"}, "seriesIndex", false);
        } else if (family == "companions") {
            server().holdOnce([](const Request &r) { return isCompanionLookup(r, 1); });
            client().refreshBooks();
        } else if (family == "series") {
            heldPath = "/series";
            server().holdPath("/series");
            client().loadSeries();
        } else if (family == "authors") {
            heldPath = "/authors";
            server().holdPath("/authors");
            client().loadAuthors();
        } else if (family == "seriesBooks") {
            server().holdOnce([](const Request &r) { return isGroupBooks(r, 1); });
            client().loadSeries();
        } else if (family == "authorBooks") {
            server().holdOnce([](const Request &r) { return isGroupBooks(r, 1); });
            client().loadAuthors();
        } else if (family == "authorSeries") {
            heldPath = "/series";
            server().holdPath("/series");
            client().showAuthorSeries("Author L1");
        }
        QTRY_COMPARE(server().heldCount(), 1);
        server().clearHolds();

        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        const QStringList rows = titles(client().books());
        QCOMPARE(client().error(), QString());

        const QJsonObject stale{{"id", 199}, {"title", "Stale L1 row"}, {"authors", QJsonArray{"Stale"}}, {"name", "Stale L1 row"}};
        const Reply success{200, QJsonDocument(QJsonObject{{"items", QJsonArray{stale}}, {"total", 1},
                                                           {"kind", "letter"}}).toJson(QJsonDocument::Compact)};
        const Reply failure{500, "{\"message\":\"Late failure\"}"};
        QVERIFY(server().release(heldPath, status == 200 ? &success : &failure));
        settle();

        QCOMPARE(client().error(), QString());
        QCOMPARE(titles(client().books()), rows);
        QVERIFY(!titles(client().booksOf()).contains("Stale L1 row"));
        QVERIFY(!names(client().seriesList()).contains("Stale L1 row"));
        QVERIFY(!names(client().authorsList()).contains("Stale L1 row"));
        QVERIFY(!names(client().authorSeriesList()).contains("Stale L1 row"));
        QVERIFY(database().listCache("series:2").isEmpty());
        QVERIFY(database().listCache("authors:2").isEmpty());
        QCOMPARE(client().currentLibraryId(), 2);
    }

    // T12
    void aToBToAKeepsOnlyTheLastRequest()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        server().holdOnce([](const Request &r) { return isPage(r, 1); });
        client().refreshBooks();
        QTRY_COMPARE(server().heldCount(), 1);
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        client().selectLibrary(1);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L1 book 1"));
        const QStringList rows = titles(client().books());
        const QJsonObject stale{{"id", 199}, {"title", "First visit row"}, {"authors", QJsonArray{"Stale"}}};
        const Reply reply{200, QJsonDocument(QJsonObject{{"items", QJsonArray{stale}}, {"total", 1}}).toJson(QJsonDocument::Compact)};
        QVERIFY(server().release("/libraries/1", &reply));
        settle();
        QCOMPARE(titles(client().books()), rows);
        QCOMPARE(client().totalBooks(), 2);
    }

    // T13
    void libraryReplyAfterSignOutIsIgnored()
    {
        twoLibraries();
        server().holdOnce([](const Request &r) { return r.path == "/libraries"; });
        client().signIn(server().url(), "one", "secret");
        QTRY_COMPARE(server().heldCount(), 1);
        client().signOut();
        server().route("GET", "/libraries", QJsonDocument(QJsonArray{QJsonObject{{"id", 2}, {"name", "Beta"}}}));
        QVERIFY(signInAndLoad("two"));
        QCOMPARE(client().currentLibraryId(), 2);
        const Reply stale{200, "[{\"id\": 1, \"name\": \"Old account's\"}]"};
        QVERIFY(server().release("/libraries", &stale));
        settle();
        QCOMPARE(client().currentLibraryId(), 2);
        QCOMPARE(client().libraries().size(), 1);
        QVERIFY(database().text(key("one")).isEmpty());
    }
    void concurrentLoadsShareOneLibraryRequest()
    {
        twoLibraries();
        server().holdPath("/libraries");
        savedSession();
        client().start();
        client().loadSeries();
        client().loadAuthors();
        client().refreshLibraries();
        QTRY_COMPARE(server().heldCount("/libraries"), 1);
        settle();
        QCOMPARE(server().countExact("GET", "/libraries"), 1);
        server().clearHolds();
        server().releaseAll();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QTRY_VERIFY(client().seriesLoaded() && client().authorsLoaded());
        QCOMPARE(server().countExact("GET", "/libraries"), 1);
    }
    void failedLibraryRequestAllowsARetry()
    {
        server().route("GET", "/libraries", 500, "{\"message\":\"Broken\"}");
        client().signIn(server().url(), USER, "secret");
        QTRY_VERIFY(client().error().contains("Broken"));
        QCOMPARE(client().currentLibraryId(), -1);
        twoLibraries();
        client().refreshBooks();
        QTRY_COMPARE(client().currentLibraryId(), 1);
    }
    void failedTokenRefreshAllowsARetry()
    {
        twoLibraries();
        server().route("POST", "/auth/refresh", 500, "{\"message\":\"Broken\"}");
        savedSession();
        client().refreshLibraries();
        QTRY_VERIFY(client().error().contains("Broken"));
        QTRY_VERIFY(!client().busy());
        server().route("POST", "/auth/refresh", QJsonDocument(QJsonObject{{"accessToken", "a"}, {"refreshToken", "r"}}));
        client().refreshLibraries();
        QTRY_COMPARE(client().currentLibraryId(), 1);
    }

    // T14
    void switchDuringTokenRefreshKeepsTheNewSelection()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().loadSeries();
        QTRY_VERIFY(!client().busy());
        server().route("POST", "/libraries/1/books/jump-buckets", 401, "{\"message\":\"Expired\"}");
        server().holdPath("/auth/refresh");
        client().refreshBooks();
        QTRY_COMPARE(server().heldCount("/auth/refresh"), 1);
        client().selectLibrary(2);
        QCOMPARE(client().currentLibraryId(), 2);
        server().clearHolds();
        server().releaseAll();
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QTRY_VERIFY(!client().busy());
        settle();
        QCOMPARE(client().currentLibraryId(), 2);
        QCOMPARE(database().text(key()), QString("2"));
        QCOMPARE(client().error(), QString());
    }

    // T15
    void collectionsAreNotTouchedByASwitch()
    {
        twoLibraries();
        server().route("GET", "/collections", QJsonDocument(QJsonArray{QJsonObject{
            {"id", 5}, {"name", "Mixed"}, {"mediaType", "books"}, {"bookCount", 0}}}));
        QVERIFY(signInAndLoad());
        client().loadCollections();
        QTRY_VERIFY(client().collectionsLoaded());
        QTRY_VERIFY(!client().busy());
        server().clearRequests();
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QVERIFY(client().collectionsLoaded());
        QCOMPARE(names(client().collections()), QStringList{"Mixed"});
        QCOMPARE(server().countContaining("/collections"), 0);
    }
    void collectionCompanionsUseTheSelectedLibrary()
    {
        twoLibraries();
        const QJsonObject book{{"id", 201}, {"title", "L2 book 1"}, {"authors", QJsonArray{"Author L2"}}};
        server().route("POST", "/collections/5/books/query", QJsonDocument(QJsonObject{{"items", QJsonArray{book}}, {"total", 1}}));
        QVERIFY(signInAndLoad());
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QTRY_VERIFY(!client().busy());
        server().clearRequests();
        client().showBooksOf("collection", {"5"}, "collectionOrder", false);
        QTRY_COMPARE(titles(client().booksOf()), QStringList{"L2 book 1"});
        QCOMPARE(server().count("POST", "/libraries/2/books"), 1);
        QCOMPARE(server().count("POST", "/books/query"), 0);
    }

    // T16, narrowed: Find books keeps its route. withLibrary shares the one resolver.
    void withLibraryWaitsForTheResolver()
    {
        twoLibraries();
        server().holdPath("/libraries");
        savedSession();
        database().setText(key(), "2");
        client().refreshLibraries();
        int got = 0;
        client().withLibrary([&](int id) { got = id; }, [](int, const QString &) { QFAIL("no error expected"); });
        QTRY_COMPARE(server().heldCount("/libraries"), 1);
        server().clearHolds();
        server().releaseAll();
        QTRY_COMPARE(got, 2);
        QCOMPARE(server().countExact("GET", "/libraries"), 1);
    }
    void withLibraryReportsAFailedResolve()
    {
        server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));
        savedSession();
        QString message;
        client().withLibrary([](int) { QFAIL("no library expected"); }, [&](int, const QString &text) { message = text; });
        QTRY_VERIFY(!message.isEmpty());
    }
    void withLibraryIsToldOfSignOut()
    {
        twoLibraries();
        server().holdPath("/libraries");
        savedSession();
        QString message;
        client().withLibrary([](int) { QFAIL("no library expected"); }, [&](int, const QString &text) { message = text; });
        QTRY_COMPARE(server().heldCount("/libraries"), 1);
        client().signOut();
        QVERIFY(!message.isEmpty());
        server().clearHolds();
        server().releaseAll();
        settle();
    }

    // T16: the real BookFinder, on a synthetic file
    void findBooksStaysOnItsLibraryAcrossASwitch()
    {
        const FindFile file;
        twoLibraries();
        QVERIFY(signInAndLoad());
        Downloads downloads(&client(), &database());
        BookFinder finder(&client(), &downloads, &database());
        server().clearRequests();
        server().holdOnce([](const Request &r) { return isPage(r, 1) && r.body.value("pagination").toObject().value("size").toInt() > 40; });
        finder.start();
        QTRY_COMPARE(server().heldCount(), 1);
        client().selectLibrary(2);
        QCOMPARE(finder.state(), QString("searching"));
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        server().clearHolds();
        server().releaseAll();
        QTRY_VERIFY(finder.state() != "searching");
        QVERIFY(finder.state() != "failed");
        QCOMPARE(server().count("POST", "/books/query"), 0);
        QCOMPARE(client().currentLibraryId(), 2);
        QCOMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        // The next run uses the new library
        server().clearRequests();
        finder.cancel();
        finder.start();
        QTRY_VERIFY(finder.state() != "searching");
        QVERIFY(server().requests.size() > 0);
        QCOMPARE(server().count("POST", "/libraries/1/books"), 0);
    }
    void findBooksJoinsTheOneLibraryRequest()
    {
        const FindFile file;
        twoLibraries();
        server().holdPath("/libraries");
        savedSession();
        database().setText(key(), "2");
        Downloads downloads(&client(), &database());
        BookFinder finder(&client(), &downloads, &database());
        finder.start();
        client().refreshLibraries();
        QTRY_COMPARE(server().heldCount("/libraries"), 1);
        server().clearHolds();
        server().releaseAll();
        QTRY_VERIFY(finder.state() != "searching");
        QCOMPARE(server().countExact("GET", "/libraries"), 1);
        QVERIFY(server().count("POST", "/libraries/2/books") > 0);
        QCOMPARE(server().count("POST", "/libraries/1/books"), 0);
    }
    void findBooksForbiddenAfterASwitchFailsWithoutARefetch()
    {
        const FindFile file;
        twoLibraries();
        QVERIFY(signInAndLoad());
        Downloads downloads(&client(), &database());
        BookFinder finder(&client(), &downloads, &database());
        server().holdOnce([](const Request &r) { return isPage(r, 1); });
        finder.start();
        QTRY_COMPARE(server().heldCount(), 1);
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QTRY_VERIFY(!client().busy());
        server().clearRequests();
        const Reply forbidden{403, "{\"message\":\"Forbidden\"}"};
        QVERIFY(server().release("/libraries/1", &forbidden));
        QTRY_VERIFY(!finder.error().isEmpty());
        QCOMPARE(server().countExact("GET", "/libraries"), 0);
        QCOMPARE(client().currentLibraryId(), 2);
    }
    void findBooksWaitingAtSignOutStops()
    {
        const FindFile file;
        twoLibraries();
        server().holdPath("/libraries");
        savedSession();
        Downloads downloads(&client(), &database());
        BookFinder finder(&client(), &downloads, &database());
        finder.start();
        QTRY_COMPARE(server().heldCount("/libraries"), 1);
        client().signOut();
        QVERIFY(!finder.error().isEmpty());
        server().clearHolds();
        server().releaseAll();
        settle();
        QCOMPARE(server().count("POST", "/libraries/"), 0);
    }

    // T17, T22
    void storedBooksAreUnchangedBySwitchFallbackAndEmpty()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        // Matching what the server lists, as normal list updates refresh saved entries; this
        // checks the switch itself adds nothing
        const auto entry = [](int library, int n) {
            return QVariantMap{{"id", library * 100 + n}, {"title", QString("L%1 book %2").arg(library).arg(n)},
                               {"authors", QStringList{QString("Author L%1").arg(library)}}};
        };
        database().saveBook(101, entry(1, 1));
        database().saveBook(201, entry(2, 1));
        QDir().mkpath(FLASHDIR);
        for (const char *name : {FLASHDIR "/a.epub", FLASHDIR "/b.epub"}) {
            QFile file(name);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(name);
        }
        database().addBookFile(101, FLASHDIR "/a.epub");
        database().addBookFile(201, FLASHDIR "/b.epub");
        database().saveBookDetails(101, QVariantMap{{"description", "Kept"}});
        const auto snapshot = [this] {
            QStringList rows;
            for (const Database::Book &book : database().booksWithFiles()) {
                QFile file(book.files.value(0));
                file.open(QIODevice::ReadOnly);
                rows.append(QString("%1 %2 %3 %4 %5").arg(book.id).arg(book.entry.value("title").toString(),
                    book.files.join(','), database().bookDetails(book.id).value("description").toString(),
                    QString::fromUtf8(file.readAll())));
            }
            return rows;
        };
        const QStringList before = snapshot();
        QCOMPARE(before.size(), 2);

        client().selectLibrary(2);   // switch
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QTRY_VERIFY(!client().busy());
        QCOMPARE(snapshot(), before);

        server().setLibraries({{1, "Alpha"}});   // fallback
        client().refreshLibraries();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        QTRY_VERIFY(!client().busy());
        QCOMPARE(snapshot(), before);

        server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));   // empty
        client().refreshLibraries();
        QTRY_COMPARE(client().currentLibraryId(), -1);
        QCOMPARE(snapshot(), before);

        newClient();   // restart
        client().start();
        QTRY_VERIFY(!client().error().isEmpty());
        QCOMPARE(snapshot(), before);
    }
    void downloadsAndSessionSurviveASwitchAndRestart()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        const QString refreshToken = database().text("refreshToken");
        database().setText("someSetting", "kept");
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        QTRY_VERIFY(!client().busy());
        newClient();
        QCOMPARE(database().text("refreshToken"), refreshToken);
        QCOMPARE(database().text("someSetting"), QString("kept"));
        QVERIFY(client().signedIn());
    }
    void offlineListsEveryDownloadAndKeepsTheChoice()
    {
        twoLibraries();
        database().setText(key(), "2");
        qputenv("PBOOKORBIT_OFFLINE", "1");
        savedSession();
        database().setText(key(), "2");
        client().setLocalBooks({localBook(101, "Downloaded A"), localBook(201, "Downloaded B")});
        client().setBookQuery("title", false, {}, true);
        client().start();
        QCOMPARE(titles(client().books()), (QStringList{"Downloaded A", "Downloaded B"}));
        QVERIFY(client().libraries().isEmpty());
        QCOMPARE(database().text(key()), QString("2"));
        client().refreshLibraries();
        settle();
        QVERIFY(server().requests.isEmpty());
    }

    // T18
    void selectingDoesNothingWhenItCannotApply()
    {
        twoLibraries();
        client().selectLibrary(2);   // signed out
        QVERIFY(server().requests.isEmpty());
        QCOMPARE(client().currentLibraryId(), -1);

        QVERIFY(signInAndLoad());
        server().clearRequests();
        QSignalSpy switched(&client(), &BookOrbitClient::librarySwitched);
        client().selectLibrary(1);    // already current
        client().selectLibrary(77);   // not listed
        settle();
        QVERIFY(server().requests.isEmpty());
        QCOMPARE(switched.count(), 0);
        QCOMPARE(database().text(key()), QString("1"));

        server().stop();
        client().refreshBooks();
        QTRY_VERIFY(!client().online());
        client().selectLibrary(2);   // offline
        QCOMPARE(client().currentLibraryId(), 1);
        QCOMPARE(database().text(key()), QString("1"));
    }

    // T19
    void offlineStartResolvesOnceOnline()
    {
        twoLibraries();
        savedSession();
        client().setLocalBooks({localBook(101, "Downloaded A")});
        client().setBookQuery("title", false, {}, true);
        server().stop();
        client().start();
        client().refreshLibraries();
        QTRY_VERIFY(!client().online());
        QVERIFY(client().libraries().isEmpty());
        server().resume();
        client().goOnline();
        QTRY_VERIFY(client().online());
        QTRY_COMPARE(client().libraries().size(), 2);
        QCOMPARE(client().currentLibraryId(), 1);
        QCOMPARE(titles(client().books()), QStringList{"Downloaded A"});
    }

    // Genres (category-browser contract T1-T21)

    // T1: the selected library's books with genres, a page at a time, as one row per genre
    void genresListTheSelectedLibrarysGenres()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        client().selectLibrary(2);
        QTRY_COMPARE(titles(client().books()).value(0), QString("L2 book 1"));
        // An ebook and its audiobook, then another book
        routeGenres(2, {genrePage({genreBook(1, "Dune", QJsonArray{"Sci-Fi", "Classic"}),
                                   genreBook(2, "Dune", QJsonArray{"Sci-Fi"}, "m4b")}, 3),
                        genrePage({genreBook(3, "Emma", QJsonArray{"Classic"})}, 3)});
        server().clearRequests();
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QCOMPARE(names(client().genresList()), (QStringList{"Classic", "Sci-Fi"}));
        const QVariantMap sciFi = rowNamed(client().genresList(), "Sci-Fi");
        QCOMPARE(sciFi.value("bookCount").toInt(), 1);
        QCOMPARE(rowNamed(client().genresList(), "Classic").value("bookCount").toInt(), 2);
        QStringList formats = sciFi.value("formats").toStringList();
        formats.sort();
        QCOMPARE(formats, (QStringList{"epub", "m4b"}));
        QCOMPARE(sciFi.value("readProgress").toDouble(), 0.0);
        QCOMPARE(client().genresProgress(), 3);
        QCOMPARE(client().genresTotal(), 3);
        QVERIFY(client().genresLoaded());
        QList<Request> pages;
        for (const Request &request : std::as_const(server().requests)) {
            QVERIFY(!isGenreList(request, 1));
            if (isGenreList(request, 2))
                pages.append(request);
        }
        QCOMPARE(pages.size(), 2);
        QCOMPARE(pageOf(pages.at(1)), 1);
        const QJsonObject body = pages.at(0).body;
        // As sent: BookOrbit's validator refuses "value": null, so isNotEmpty has no value
        const QJsonObject filter{{"type", "group"}, {"join", "AND"},
            {"rules", QJsonArray{QJsonObject{{"type", "rule"}, {"field", "genre"}, {"operator", "isNotEmpty"}}}}};
        QCOMPARE(body.value("filter").toObject(), filter);
        QCOMPARE(body.value("pagination").toObject().value("size").toInt(), 200);
        QCOMPARE(body.value("sort").toArray().at(0).toObject().value("field").toString(), QString("title"));
        QCOMPARE(body.value("sort").toArray().at(0).toObject().value("dir").toString(), QString("asc"));
        QStringList cached = cachedNames(database().listCache("genres:2"));
        cached.sort();
        QCOMPARE(cached, (QStringList{"Classic", "Sci-Fi"}));
        client().setGenreSort("bookCount", true);
        QCOMPARE(names(client().genresList()), (QStringList{"Classic", "Sci-Fi"}));
        client().setGenreSort("name", true);
        QCOMPARE(names(client().genresList()), (QStringList{"Sci-Fi", "Classic"}));
    }

    // T2: a genre's books are the library's with exactly that genre
    void openingAGenreQueriesItsExactName()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        server().clearRequests();
        client().showBooksOf("genre", {"Sci-Fi"}, "title", false);
        const auto genreRule = [this] {
            for (const Request &request : std::as_const(server().requests)) {
                const QJsonObject first = request.body.value("filter").toObject().value("rules").toArray().at(0).toObject();
                if (request.path == "/libraries/1/books" && first.value("field") == "genre")
                    return first;
            }
            return QJsonObject();
        };
        QTRY_VERIFY(!genreRule().isEmpty());
        QCOMPARE(genreRule().value("operator").toString(), QString("includesAny"));
        QCOMPARE(genreRule().value("value").toArray(), QJsonArray{"Sci-Fi"});
    }

    // T3: offline, a genre's books are the downloaded books with it, never an author's
    void offlineGenreBooksMatchGenresOnly()
    {
        qputenv("PBOOKORBIT_OFFLINE", "1");
        savedSession();
        QVariantMap genred = localBook(101, "Has the genre");
        genred.insert("genres", QStringList{"Sci-Fi"});
        QVariantMap byAuthor = localBook(102, "Author named like it");
        byAuthor.insert("authors", QStringList{"Sci-Fi"});
        const QVariantMap noKey = localBook(103, "Saved without genres");
        client().setLocalBooks({genred, byAuthor, noKey});
        client().start();
        client().showBooksOf("genre", {"Sci-Fi"}, "title", false, {}, true);
        QCOMPARE(titles(client().booksOf()), QStringList{"Has the genre"});
        client().showBooksOf("author", {"Sci-Fi"}, "title", false, {}, true);
        QCOMPARE(titles(client().booksOf()), QStringList{"Author named like it"});
        client().showBooksOf("publisher", {"Sci-Fi"}, "title", false, {}, true);
        QCOMPARE(titles(client().booksOf()), QStringList());
    }

    // T4, T18: offline, the downloaded books' genres, kept up to date as downloads change
    void offlineGenresAreTheDownloadedBooks()
    {
        qputenv("PBOOKORBIT_OFFLINE", "1");
        savedSession();
        QVariantMap first = localBook(101, "First");
        first.insert("genres", QStringList{"Sci-Fi"});
        first.insert("downloadedFormats", QStringList{"epub"});
        const QVariantMap noKey = localBook(102, "Saved without genres");
        client().setLocalBooks({first, noKey});
        client().start();
        client().loadGenres();
        QCOMPARE(client().genresStatus(), QString("local"));
        QVERIFY(client().genresLoaded());
        QCOMPARE(names(client().genresList()), QStringList{"Sci-Fi"});
        QCOMPARE(rowNamed(client().genresList(), "Sci-Fi").value("bookCount").toInt(), 1);

        QVariantMap second = localBook(103, "Second");
        second.insert("genres", QStringList{"Poetry"});
        client().setLocalBooks({first, noKey, second});
        QCOMPARE(names(client().genresList()), (QStringList{"Poetry", "Sci-Fi"}));
        client().setLocalBooks({noKey, second});
        QCOMPARE(names(client().genresList()), QStringList{"Poetry"});
        settle();
        QVERIFY(server().requests.isEmpty());
    }

    // T6: per-library caches, written only when complete, cleared on sign-out
    void genreCachesBelongToTheirLibrary()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        routeGenres(1, {genrePage({genreBook(1, "A1", QJsonArray{"Only in A"})}, 1)});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QCOMPARE(cachedNames(database().listCache("genres:1")), QStringList{"Only in A"});

        routeGenres(2, {genrePage({genreBook(2, "B1", QJsonArray{"Only in B"})}, 1)});
        server().holdOnce([](const Request &r) { return isGenreList(r, 2); });
        client().selectLibrary(2);
        QCOMPARE(client().genresStatus(), QString("idle"));
        QCOMPARE(client().genresList()->count(), 0);
        client().loadGenres();
        QVERIFY(!names(client().genresList()).contains("Only in A"));
        QTRY_COMPARE(server().heldCount(), 1);
        server().releaseAll();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QCOMPARE(names(client().genresList()), QStringList{"Only in B"});
        QCOMPARE(cachedNames(database().listCache("genres:1")), QStringList{"Only in A"});

        client().signOut();
        QTRY_VERIFY(!client().signedIn());
        QVERIFY(database().listCache("genres:1").isEmpty());
        QVERIFY(database().listCache("genres:2").isEmpty());
        QCOMPARE(client().genresList()->count(), 0);
    }

    // T8: names are exact; a book counts once per genre; entries that aren't names are skipped
    void genreNamesAreExactAndCountedOncePerBook()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        routeGenres(1, {genrePage({genreBook(1, "One", QJsonArray{"Sci-Fi", "sci-fi", "Sci-Fi", QJsonValue(),
                                                                     7, QJsonObject{}, "", "  ", " Sci-Fi"}),
                                   genreBook(2, "Two", QJsonObject{{"Sci-Fi", true}}),
                                   genreBook(3, "Three", QJsonValue::Undefined)}, 3)});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QStringList found = names(client().genresList());
        found.sort();
        QCOMPARE(found, (QStringList{" Sci-Fi", "Sci-Fi", "sci-fi"}));
        QCOMPARE(rowNamed(client().genresList(), "Sci-Fi").value("bookCount").toInt(), 1);
        QCOMPARE(rowNamed(client().genresList(), "sci-fi").value("bookCount").toInt(), 1);
    }

    // T12: an empty page before the total is reached isn't the end of the list
    void anEarlyEmptyPageLeavesGenresIncomplete()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        QJsonArray items;
        for (int i = 0; i < 200; i++)
            items.append(genreBook(1000 + i, QString("Book %1").arg(i), QJsonArray{"Sci-Fi"}));
        routeGenres(1, {genrePage(items, 500), genrePage({}, 500)});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("incomplete"));
        QCOMPARE(names(client().genresList()), QStringList{"Sci-Fi"});
        QCOMPARE(client().genresProgress(), 200);
        QCOMPARE(client().genresTotal(), 500);
        QVERIFY(database().listCache("genres:1").isEmpty());
    }

    // T24: no books with genres is a complete, empty list
    void noGenresIsACompleteEmptyList()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        routeGenres(1, {genrePage({}, 0)});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QCOMPARE(client().genresList()->count(), 0);
        QVERIFY(client().genresLoaded());
        QVERIFY(database().listCache("genres:1").contains("rows"));
    }

    // T13: a page that isn't a valid list fails, once, and isn't cached
    void invalidGenrePagesFail_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addRow("not JSON") << QByteArray("not json");
        QTest::addRow("array") << QByteArray("[]");
        QTest::addRow("items not an array") << QByteArray("{\"items\":{},\"total\":1}");
        QTest::addRow("items null") << QByteArray("{\"items\":null,\"total\":1}");
        QTest::addRow("total missing") << QByteArray("{\"items\":[]}");
        QTest::addRow("total negative") << QByteArray("{\"items\":[],\"total\":-1}");
        QTest::addRow("total text") << QByteArray("{\"items\":[],\"total\":\"3\"}");
        QTest::addRow("total fractional") << QByteArray("{\"items\":[],\"total\":1.5}");
        QTest::addRow("total above int") << QByteArray("{\"items\":[],\"total\":2147483648}");
        QTest::addRow("total huge") << QByteArray("{\"items\":[],\"total\":1e300}");
    }
    void invalidGenrePagesFail()
    {
        QFETCH(QByteArray, body);
        twoLibraries();
        QVERIFY(signInAndLoad());
        QTRY_VERIFY(!client().busy());
        routeGenres(1, {Reply{200, body}});
        QSignalSpy errors(&client(), &BookOrbitClient::errorChanged);
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("failed"));
        QCOMPARE(client().error(), QString("Unexpected response from server"));
        QCOMPARE(errors.count(), 1);
        QCOMPARE(client().genresList()->count(), 0);
        QVERIFY(client().genresLoaded());
        QVERIFY(database().listCache("genres:1").isEmpty());
    }

    // T14: replies to a load that a newer one replaced change nothing
    void supersededGenreLoadsAreIgnored()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        QTRY_VERIFY(!client().busy());
        routeGenres(1, {genrePage({genreBook(1, "New", QJsonArray{"Newest"})}, 1)});
        server().holdOnce([](const Request &r) { return isGenreList(r, 1); });
        server().holdOnce([](const Request &r) { return isGenreList(r, 1); });
        client().loadGenres();
        client().loadGenres();
        QTRY_COMPARE(server().heldCount(), 2);
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QCOMPARE(names(client().genresList()), QStringList{"Newest"});
        const Reply failure{500, "{\"message\":\"Late failure\"}"};
        const Reply malformed{200, "{\"items\":7}"};
        QVERIFY(server().release("/libraries/1/books", &failure));
        QVERIFY(server().release("/libraries/1/books", &malformed));
        settle();
        QCOMPARE(client().error(), QString());
        QCOMPARE(client().genresStatus(), QString("complete"));
        QCOMPARE(names(client().genresList()), QStringList{"Newest"});
        QCOMPARE(cachedNames(database().listCache("genres:1")), QStringList{"Newest"});
    }

    // T15: a reply held across A -> B -> A belongs to the first visit only
    void aGenreReplyFromAnEarlierVisitIsIgnored()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        routeGenres(1, {genrePage({genreBook(1, "Fresh", QJsonArray{"Fresh"})}, 1)});
        routeGenres(2, {genrePage({genreBook(2, "B", QJsonArray{"B genre"})}, 1)});
        server().holdOnce([](const Request &r) { return isGenreList(r, 1); });
        client().loadGenres();
        QTRY_COMPARE(server().heldCount(), 1);
        client().selectLibrary(2);
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        client().selectLibrary(1);
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QCOMPARE(names(client().genresList()), QStringList{"Fresh"});
        const Reply stale = genrePage({genreBook(9, "Stale", QJsonArray{"Stale"})}, 1);
        QVERIFY(server().release("/libraries/1/books", &stale));
        settle();
        QCOMPARE(names(client().genresList()), QStringList{"Fresh"});
        QCOMPARE(cachedNames(database().listCache("genres:1")), QStringList{"Fresh"});
        QCOMPARE(cachedNames(database().listCache("genres:2")), QStringList{"B genre"});
    }

    // T16, T21: a failed page leaves the rows that loaded; a retry completes and clears its
    // error, but not someone else's
    void aPartialGenreListRetries()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        QTRY_VERIFY(!client().busy());
        const Reply first = genrePage({genreBook(1, "One", QJsonArray{"First page"})}, 2);
        routeGenres(1, {first, Reply{500, "{\"message\":\"Genre failure\"}"}});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("incomplete"));
        QCOMPARE(names(client().genresList()), QStringList{"First page"});
        QVERIFY(client().error().contains("Genre failure"));
        QVERIFY(database().listCache("genres:1").isEmpty());

        routeGenres(1, {first, genrePage({genreBook(2, "Two", QJsonArray{"Second page"})}, 2)});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QCOMPARE(names(client().genresList()), (QStringList{"First page", "Second page"}));
        QCOMPARE(client().error(), QString());
        QCOMPARE(cachedNames(database().listCache("genres:1")).size(), 2);

        // Another view's error, set after the genres', stays
        routeGenres(1, {Reply{500, "{\"message\":\"Genre failure\"}"}});
        client().loadGenres();
        QTRY_VERIFY(genresSettled());
        QVERIFY(client().error().contains("Genre failure"));
        server().routeWhen([](const Request &r) { return r.path.startsWith("/series"); },
            [](const Request &) { return Reply{500, "{\"message\":\"Series failure\"}"}; });
        client().loadSeries();
        QTRY_VERIFY(client().error().contains("Series failure"));
        routeGenres(1, {genrePage({genreBook(1, "One", QJsonArray{"First page"})}, 1)});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QVERIFY(client().error().contains("Series failure"));
    }

    // T17: a failed reload keeps the last complete list on screen and on disk
    void aFailedReloadKeepsTheCompleteList()
    {
        twoLibraries();
        QVERIFY(signInAndLoad());
        const QVariantMap cache{{"rows", QVariantList{QVariantMap{{"id", 0}, {"name", "Cached"}, {"bookCount", 4}}}}};
        database().saveListCache("genres:1", cache);
        routeGenres(1, {Reply{500, "{\"message\":\"Genre failure\"}"}});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("stale"));
        QCOMPARE(names(client().genresList()), QStringList{"Cached"});
        QCOMPARE(database().listCache("genres:1"), cache);

        // Also after a complete load this session
        routeGenres(1, {genrePage({genreBook(1, "One", QJsonArray{"Loaded"})}, 1)});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        routeGenres(1, {Reply{500, "{\"message\":\"Genre failure\"}"}});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("stale"));
        QCOMPARE(names(client().genresList()), QStringList{"Loaded"});
        QCOMPARE(cachedNames(database().listCache("genres:1")), QStringList{"Loaded"});
    }

    // Review R1: a complete empty list is a complete list too, kept while reloading and when
    // the reload fails
    void anEmptyCompleteListSurvivesAFailedReload_data()
    {
        QTest::addColumn<bool>("firstPageFails");
        QTest::addRow("first page fails") << true;
        QTest::addRow("later page fails") << false;
    }
    void anEmptyCompleteListSurvivesAFailedReload()
    {
        QFETCH(bool, firstPageFails);
        twoLibraries();
        QVERIFY(signInAndLoad());
        routeGenres(1, {genrePage({}, 0)});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        const QVariantMap empty = database().listCache("genres:1");
        QVERIFY(empty.contains("rows"));

        const Reply failure{500, "{\"message\":\"Genre failure\"}"};
        const Reply partial = genrePage({genreBook(1, "One", QJsonArray{"Partial"})}, 2);
        routeGenres(1, {firstPageFails ? failure : partial, failure});
        server().holdOnce([](const Request &r) { return isGenreList(r, 1) && pageOf(r) == 1; });
        client().loadGenres();
        QCOMPARE(client().genresStatus(), QString("loading"));
        QVERIFY(client().genresCached());
        if (!firstPageFails) {
            // The first page's rows wait for the rest, behind the complete empty list
            QTRY_COMPARE(server().heldCount(), 1);
            QCOMPARE(client().genresList()->count(), 0);
            QVERIFY(client().genresCached());
            server().releaseAll();
        }
        QTRY_COMPARE(client().genresStatus(), QString("stale"));
        QVERIFY(client().genresCached());
        QCOMPARE(client().genresList()->count(), 0);
        QCOMPARE(database().listCache("genres:1"), empty);

        // A reload that completes replaces it
        server().clearHolds();
        routeGenres(1, {partial, genrePage({genreBook(2, "Two", QJsonArray{"More"})}, 2)});
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QVERIFY(!client().genresCached());
        QCOMPARE(names(client().genresList()), (QStringList{"More", "Partial"}));
        QCOMPARE(cachedNames(database().listCache("genres:1")).size(), 2);
    }

    // T20: a failed or empty library list settles the genres without their own error, and
    // they load once libraries come back
    void genresSettleWhenLibrariesFail_data()
    {
        QTest::addColumn<bool>("empty");
        QTest::addRow("server error") << false;
        QTest::addRow("no libraries") << true;
    }
    void genresSettleWhenLibrariesFail()
    {
        QFETCH(bool, empty);
        if (empty)
            server().route("GET", "/libraries", QJsonDocument(QJsonArray{}));
        else
            server().route("GET", "/libraries", 500, "{\"message\":\"Broken\"}");
        client().signIn(server().url(), USER, "secret");
        QTRY_VERIFY(!client().error().isEmpty());
        const QString libraryError = client().error();
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("failed"));
        QCOMPARE(client().error(), libraryError);
        QVERIFY(client().genresLoaded());

        twoLibraries();
        routeGenres(1, {genrePage({genreBook(1, "One", QJsonArray{"Back"})}, 1)});
        client().refreshLibraries();
        QTRY_COMPARE(client().currentLibraryId(), 1);
        client().loadGenres();
        QTRY_COMPARE(client().genresStatus(), QString("complete"));
        QCOMPARE(names(client().genresList()), QStringList{"Back"});
    }
};

QTEST_MAIN(LibraryTest)
#include "tst_library.moc"
