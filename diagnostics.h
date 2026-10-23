#pragma once

#include <QString>

class QQuickWindow;

// A small log for finding slowdowns on the device: /mnt/ext1/bookorbit.log, at the root
// of the device's USB storage (./system/mnt/ext1 in the emulator). It is cleared each
// time the app starts.
namespace diagnostics {

void log(const QString &message);

// Every 10 seconds, logs how many frames the window rendered and the app's memory and
// thread count. On e-ink each frame is a screen update, so frames should be 0 when idle.
void start(QQuickWindow *window);

}
