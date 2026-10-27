#pragma once

#include <QObject>
#include <QString>

class QQuickWindow;

// A small log for finding slowdowns on the device: /mnt/ext1/bookorbit.log, at the root
// of the device's USB storage (./system/mnt/ext1 in the emulator). It is cleared each
// time the app starts.
namespace diagnostics {

void log(const QString &message);

// Sends Qt's own warnings (QML errors included) to the log too. The platform plugin
// otherwise swallows them.
void captureQtMessages();

// Every 10 seconds, logs how many frames the window rendered and the app's memory and
// thread count. On e-ink each frame is a screen update, so frames should be 0 when idle.
void start(QQuickWindow *window);

// diagnostics.log() for QML
class Log : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE void log(const QString &message) { diagnostics::log(message); }
};

}
