#include "core/diagnostics.h"

#include "core/database.h"
#include <common_utilities/einkinterface.h>
#include <inkview.h>
#include <QDateTime>
#include <QFile>
#include <QtLogging>
#include <QQuickWindow>
#include <QSet>
#include <QTimer>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace diagnostics {

static const char *const SETTING = "diagnosticsLogging";

static bool tracing = false;
// Frame and memory counter, and the network watch, running while tracing
static QTimer *statsTimer = nullptr;
static QTimer *networkTimer = nullptr;

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

QString networkState()
{
    const int state = QueryNetwork();
    QStringList flags;
    const QList<QPair<int, const char *>> names{
        {NET_BLUETOOTH, "bluetooth"}, {NET_WIFI, "wifi"}, {NET_CDMA3G, "3g"},
        {NET_BTREADY, "bluetooth ready"}, {NET_WIFIREADY, "wifi ready"}, {NET_CDMA3GREADY, "3g ready"}};
    for (const auto &[flag, name] : names) {
        if (state & flag)
            flags << QString::fromLatin1(name);
    }
    return QString("net 0x%1 (%2)").arg(state, 4, 16, QChar('0')).arg(flags.join(", "));
}

static void setTracing(bool on)
{
    tracing = on;
    for (QTimer *timer : {statsTimer, networkTimer}) {
        if (timer && on)
            timer->start();
        else if (timer)
            timer->stop();
    }
}

void start(QQuickWindow *window)
{
    // afterRendering fires on the render thread
    static std::atomic<int> frames{0};
    QObject::connect(window, &QQuickWindow::afterRendering, window, [] { frames++; },
                     Qt::DirectConnection);

    statsTimer = new QTimer(window);
    statsTimer->setInterval(10000);
    QObject::connect(statsTimer, &QTimer::timeout, [] {
        log(QString("frames=%1 %2 %3").arg(frames.exchange(0)).arg(processStats(), networkState()));
    });
    networkTimer = new QTimer(window);
    networkTimer->setInterval(2000);
    QObject::connect(networkTimer, &QTimer::timeout, [] {
        static QString last = networkState();
        const QString state = networkState();
        if (state != last)
            log("network changed: " + last + " -> " + state);
        last = state;
    });
    setTracing(tracing);

    // Which of the system's events reach the app, Wi-Fi's in particular. Handed to the main
    // thread, since it may not be the one calling.
    if (auto *platform = pocketbook::utilities::einkPlatform()) {
        platform->addHandlerForInkvewEvents([window](int type, int par1, int par2) {
            QMetaObject::invokeMethod(window, [type, par1, par2] {
                static QSet<int> seen;
                if (type < 200 && seen.contains(type))
                    return;
                seen.insert(type);
                log(QString("inkview event %1 (%2, %3), %4").arg(type).arg(par1).arg(par2).arg(networkState()));
            }, Qt::QueuedConnection);
        });
    }
    log("started, " + processStats() + (tracing ? ", diagnostics logging on" : ""));
    log(networkState());
    // QML is compiled at build time for this exact Qt version. Any other version ignores it
    // and compiles from source on every launch.
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
