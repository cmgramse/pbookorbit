#pragma once

#include <QObject>
#include <QString>

class Database;
class QQuickWindow;

// A small log for bug reports and finding slowdowns on the device: /mnt/ext1/pbookorbit.log,
// at the root of the device's USB storage (./system/mnt/ext1 in the emulator). It is
// cleared each time the app starts.
//
// log() always writes, for errors and what the app found out at startup. trace() writes
// only while diagnostics logging is on, for the detail needed while developing: every
// request, e-ink updates, frame counts. Tapping the title six times turns it on or off.
namespace diagnostics {

void log(const QString &message);
void trace(const QString &message);

// Sends Qt's own warnings (QML errors included) to the log too. The platform plugin
// otherwise swallows them.
void captureQtMessages();

// Logs the app's startup details. While diagnostics logging is on, every 10 seconds it
// also logs how many frames the window rendered and the app's memory and thread count. On
// e-ink each frame is a screen update, so frames should be 0 when idle.
void start(QQuickWindow *window);

// For QML: diagnostics.log(), diagnostics.trace(), and turning diagnostics logging on and
// off. Whether it is on is kept in the settings.
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
