#pragma once

#include <QObject>
#include <QString>

class Database;
class QQuickWindow;

// A small log for bug reports and tracking down slowdowns: /mnt/ext1/pbookorbit.log at the
// root of the device's storage (./system/mnt/ext1 in the emulator). Cleared on each start.
//
// log() always writes, for errors and startup findings. trace() only writes while
// diagnostics logging is on, for development detail such as requests, e-ink updates and
// frame counts. Tapping the title six times toggles it.
namespace diagnostics {

void log(const QString &message);
void trace(const QString &message);

// Also sends Qt's warnings, including QML errors, to the log. The platform plugin swallows
// them otherwise.
void captureQtMessages();

// Logs startup details. While diagnostics logging is on, also logs frames rendered, memory
// and thread count every 10 seconds. On e-ink each frame is a screen update, so frames
// should be 0 when idle.
void start(QQuickWindow *window);

// QML access to log(), trace() and the diagnostics logging switch, which is saved in
// settings
class Log : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
public:
    explicit Log(Database *database, QObject *parent = nullptr);
    bool enabled() const;
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void log(const QString &message) { diagnostics::log(message); }
    Q_INVOKABLE void trace(const QString &message) { diagnostics::trace(message); }

signals:
    void enabledChanged();

private:
    Database *m_database;
};

}
