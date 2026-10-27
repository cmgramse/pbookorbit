#include "core/diagnostics.h"

#include "core/database.h"
#include <inkview.h>
#include <QDateTime>
#include <QFile>
#include <QtLogging>
#include <QQuickWindow>
#include <QTimer>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace diagnostics {

static const char *const SETTING = "diagnosticsLogging";

static bool tracing = false;
// The frame and memory counter, run while tracing
static QTimer *statsTimer = nullptr;

static QFile *logFile()
{
    static QFile *file = [] {
        auto *f = new QFile(QString::fromUtf8(FLASHDIR "/pbookorbit.log"));
        if (!f->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            fprintf(stderr, "pbookorbit: can't write %s\n", qPrintable(f->fileName()));
        return f;
    }();
    return file;
}

void log(const QString &message)
{
    const QByteArray line = QDateTime::currentDateTime().toString("hh:mm:ss.zzz ").toUtf8()
                            + message.toUtf8() + '\n';
    QFile *file = logFile();
    if (file->isOpen()) {
        file->write(line);
        file->flush();
    } else {
        fputs(line.constData(), stderr);
    }
}

void trace(const QString &message)
{
    if (tracing)
        log(message);
}

void captureQtMessages()
{
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message) {
        if (type != QtDebugMsg && type != QtInfoMsg)
            log("Qt: " + message);
    });
}

// VmRSS and Threads from /proc/self/status
static QString processStats()
{
    QFile status("/proc/self/status");
    if (!status.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QStringList parts;
    for (const QByteArray &line : status.readAll().split('\n')) {
        if (line.startsWith("VmRSS:") || line.startsWith("Threads:"))
            parts << QString::fromUtf8(line.simplified());
    }
    return parts.join(' ');
}

static void setTracing(bool on)
{
    tracing = on;
    if (statsTimer && on)
        statsTimer->start();
    else if (statsTimer)
        statsTimer->stop();
}

void start(QQuickWindow *window)
{
    // afterRendering is emitted on the render thread
    static std::atomic<int> frames{0};
    QObject::connect(window, &QQuickWindow::afterRendering, window, [] { frames++; },
                     Qt::DirectConnection);

    statsTimer = new QTimer(window);
    statsTimer->setInterval(10000);
    QObject::connect(statsTimer, &QTimer::timeout, [] {
        log(QString("frames=%1 %2").arg(frames.exchange(0)).arg(processStats()));
    });
    setTracing(tracing);
    log("started, " + processStats() + (tracing ? ", diagnostics logging on" : ""));
    // The QML is compiled at build time for exactly this Qt; on any other the engine
    // ignores it and compiles every file from source on each launch.
    log(QString("Qt %1, QML compiled for %2%3")
            .arg(qVersion(), QT_VERSION_STR,
                 strcmp(qVersion(), QT_VERSION_STR) ? " - mismatch, compiling QML at startup"
                                                    : ""));
}

Log::Log(Database *database, QObject *parent)
    : QObject(parent), m_database(database)
{
    setTracing(m_database->flag(SETTING));
}

bool Log::enabled() const
{
    return tracing;
}

void Log::toggle()
{
    setTracing(!tracing);
    m_database->setFlag(SETTING, tracing);
    log(tracing ? "diagnostics logging on" : "diagnostics logging off");
    emit enabledChanged();
}

}
