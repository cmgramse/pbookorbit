#include "core/diagnostics.h"

#include <inkview.h>
#include <QDateTime>
#include <QFile>
#include <QtLogging>
#include <QQuickWindow>
#include <QTimer>
#include <atomic>
#include <cstdio>

namespace diagnostics {

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

void start(QQuickWindow *window)
{
    // afterRendering is emitted on the render thread
    static std::atomic<int> frames{0};
    QObject::connect(window, &QQuickWindow::afterRendering, window, [] { frames++; },
                     Qt::DirectConnection);

    auto *timer = new QTimer(window);
    QObject::connect(timer, &QTimer::timeout, [] {
        log(QString("frames=%1 %2").arg(frames.exchange(0)).arg(processStats()));
    });
    timer->start(10000);
    log("started, " + processStats());
}

}
