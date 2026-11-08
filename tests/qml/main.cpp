#include <QDir>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QtQuickTest>

// QML warnings, such as binding loops, which don't fail a test by themselves
static QStringList warnings;
static QtMessageHandler defaultHandler = nullptr;

static void recordWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (type == QtWarningMsg)
        warnings.append(message);
    defaultHandler(type, context, message);
}

class WarningLog : public QObject
{
    Q_OBJECT

public:
    // From each test: QtTest puts in its own handler when it starts
    Q_INVOKABLE void watch()
    {
        const QtMessageHandler previous = qInstallMessageHandler(recordWarnings);
        if (previous != recordWarnings)
            defaultHandler = previous;
    }
    Q_INVOKABLE QString take()
    {
        const QString text = warnings.join('\n');
        warnings.clear();
        return text;
    }
};

// The SDK's theme icons aren't on the host
class BlankIcons : public QQuickImageProvider
{
public:
    BlankIcons() : QQuickImageProvider(Image) {}
    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        QImage image(16, 16, QImage::Format_RGB32);
        image.fill(Qt::white);
        *size = image.size();
        return image;
    }
};

// What main.cpp gives the app, and whether each of its QML files compiles: main.qml's wiring
// isn't run by these tests, but this catches a mistake in it.
class Setup : public QObject
{
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty("dpScale", 1);
        engine->addImageProvider("theme", new BlankIcons);
        engine->rootContext()->setContextProperty("warningLog", new WarningLog);
        // main.qml needs these to run; they don't affect the other tests
        QQmlComponent services(engine, QUrl::fromLocalFile(QUICK_TEST_SOURCE_DIR "/MockServices.qml"));
        QObject *mock = services.create();
        for (const char *name : {"mockServices", "bookOrbit", "covers", "downloads", "bookFinder", "browser",
                                 "eink", "diagnostics", "launcherTile"})
            engine->rootContext()->setContextProperty(name, mock);
        engine->rootContext()->setContextProperty("screenWidth", 480);
        engine->rootContext()->setContextProperty("windowHeight", 640);
        QStringList errors;
        const QDir dir(QML_SOURCE_DIR);
        const QStringList files = dir.entryList({"*.qml"});
        for (const QString &file : files) {
            QQmlComponent component(engine, QUrl::fromLocalFile(dir.filePath(file)));
            if (!component.isReady())
                errors.append(component.errorString());
        }
        engine->rootContext()->setContextProperty("qmlFileCount", int(files.size()));
        engine->rootContext()->setContextProperty("qmlCompileErrors", errors);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(library_ui, Setup)
#include "main.moc"
