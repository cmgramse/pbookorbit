#include <inkview.h>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStyleHints>

#include "bookorbitclient.h"
#include "diagnostics.h"
#include "eink.h"

int main(int argc, char *argv[])
{
    // Connect to the PocketBook system (on PC this opens the emulator window)
    InitInkview(ACTIVETASK);

    // Render Qt through the PocketBook e-ink platform plugin
    setenv("QT_QPA_PLATFORM", "pocketbook2", true);

    QGuiApplication app(argc, argv);
    app.setFont(QFont(QString::fromUtf8(iv_get_default_font(FONT_FAMILY))));
    // A blinking cursor redraws the screen twice a second for as long as a text field has
    // focus, and every redraw is a slow e-ink update. Keep the cursor solid instead.
    app.styleHints()->setCursorFlashTime(0);

    // /mnt/ext1/system/config on the device, ./system/config in the emulator
    BookOrbitClient client(QString::fromUtf8(CONFIGPATH "/bookorbit.ini"));

    Eink eink;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("screenWidth", ScreenWidth());
    // Leave room for the system status bar, as the SDK's browser-minimal example does
    engine.rootContext()->setContextProperty("windowHeight", ScreenHeight() - PanelHeight());
    engine.rootContext()->setContextProperty("bookOrbit", &client);
    engine.rootContext()->setContextProperty("eink", &eink);
    engine.load(QUrl("qrc:/main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    diagnostics::start(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));
    diagnostics::log(QString("screen %1x%2, panel height %3")
                         .arg(ScreenWidth()).arg(ScreenHeight()).arg(PanelHeight()));
    client.start();
    return app.exec();
}
