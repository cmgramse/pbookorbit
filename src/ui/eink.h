#pragma once

#include <QObject>

// E-ink update modes for QML, via the pocketbook2 platform plugin.
//
// A2 is the fastest update (about 8 a second) but black and white only, so the screen looks
// dithered while it's on. Use it for one region while it scrolls, as the official apps do,
// then let the region redraw at full quality.
//
// Dynamic updates are faster than the default partial ones, for typing. The SDK's qml_test
// README calls this setQuickKeyboardEnterUpdateMode(), deprecated here in favour of
// setDynamicUpdateMode().
class Eink : public QObject
{
    Q_OBJECT
    // Whether system night mode is on, for images and colours that should keep their real
    // colours (night.js)
    Q_PROPERTY(bool nightMode READ nightMode NOTIFY nightModeChanged)

public:
    using QObject::QObject;

    // Lets system night mode invert the app as it does the built-in ones, and redraws on
    // toggle. InkView only inverts apps that opt in, which by default is only those in
    // /ebrmain. Call after the platform plugin has started.
    void followNightMode();
    bool nightMode() const { return m_nightMode; }

    // Switches a screen rectangle (window coordinates) to A2 for a flick. Stays in A2 until
    // leaveA2Soon() or leaveA2(). Calling again cancels a pending timed exit.
    Q_INVOKABLE void enterA2ForFlick(int x, int y, int width, int height);
    // Ends A2 and redraws the region normally once the last frames have shown. Call when
    // movement, including momentum, stops.
    Q_INVOKABLE void leaveA2Soon();
    // Ends A2 and redraws the region normally right away, e.g. when the region disappears
    Q_INVOKABLE void leaveA2();

    // Enables dynamic updates while a text field has focus. Disabling redraws the window at
    // normal quality.
    Q_INVOKABLE void setDynamicUpdates(bool enabled);

signals:
    void nightModeChanged();

private:
    bool m_dynamicUpdates = false;
    bool m_nightMode = false;
};
