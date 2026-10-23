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

    // Switches a screen rectangle (in window coordinates) to A2 for a flick. It stays in A2
    // until leaveA2() is called, so call it when the movement ends, momentum included.
    Q_INVOKABLE void enterA2ForFlick(int x, int y, int width, int height);
    // Ends A2 and redraws the region normally
    Q_INVOKABLE void leaveA2();

    // Turns dynamic updates on while a text field has focus. Turning them off redraws the
    // window at normal quality.
    Q_INVOKABLE void setDynamicUpdates(bool enabled);

private:
    bool m_dynamicUpdates = false;
};
