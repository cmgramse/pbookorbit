#pragma once

#include <QObject>

// E-ink update modes for QML, from the pocketbook2 platform plugin.
//
// A2 is the fastest e-ink update (around 8 a second) but draws only black and white, so
// the screen looks dithered while it is on. Use it for one region while that region
// scrolls, as the official apps do, then leave it to redraw the region at full quality.
//
// Dynamic updates are quicker than the default partial ones, for typing. The SDK's
// qml_test README calls this setQuickKeyboardEnterUpdateMode(), which this SDK deprecates
// in favour of setDynamicUpdateMode().
class Eink : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Lets the system's night mode invert the app, as it does the built-in apps, and
    // redraws the screen when night mode is turned on or off. InkView only inverts apps
    // that say they support it, and only the ones in /ebrmain say so by default. Call it
    // once the platform plugin has started.
    void followNightMode();

    // Switches a screen rectangle (in window coordinates) to A2 for a flick. It stays in A2
    // until leaveA2Soon() or leaveA2() is called. Calling it again before a timed exit
    // cancels the exit.
    Q_INVOKABLE void enterA2ForFlick(int x, int y, int width, int height);
    // Ends A2 and redraws the region normally once the last frames have had time to show.
    // Call it when the movement ends, momentum included.
    Q_INVOKABLE void leaveA2Soon();
    // Ends A2 and redraws the region normally now, for when the region goes away
    Q_INVOKABLE void leaveA2();

    // Turns dynamic updates on while a text field has focus. Turning them off redraws the
    // window at normal quality.
    Q_INVOKABLE void setDynamicUpdates(bool enabled);

private:
    bool m_dynamicUpdates = false;
    bool m_nightMode = false;
};
