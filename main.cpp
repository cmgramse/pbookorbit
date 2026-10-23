#include <inkview.h>
#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStyleHints>

#include "bookorbitclient.h"
#include "diagnostics.h"
#include "eink.h"
#include "themeimageprovider.h"

int main(int argc, char *argv[])
{
    // Connect to the PocketBook system (on PC this opens the emulator window)
    InitInkview(ACTIVETASK);

    // Render Qt through the PocketBook e-ink platform plugin
    setenv("QT_QPA_PLATFORM", "pocketbook2", true);

    QGuiApplication app(argc, argv);
    diagnostics::captureQtMessages();
    app.setFont(QFont(QString::fromUtf8(iv_get_default_font(FONT_FAMILY))));
    // A blinking cursor redraws the screen twice a second for as long as a text field has
    // focus, and every redraw is a slow e-ink update. Keep the cursor solid instead.
    app.styleHints()->setCursorFlashTime(0);

    // /mnt/ext1/system/config on the device, ./system/config in the emulator
    BookOrbitClient client(QString::fromUtf8(CONFIGPATH "/bookorbit.ini"));

    Eink eink;
    diagnostics::Log diagnosticsLog;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("screenWidth", ScreenWidth());
    // Pixels per dp, for units.js: the SDK's qml_test example scales sizes the same way
    engine.rootContext()->setContextProperty("dpScale", get_screen_dpi() * get_screen_scale_factor() / 160.0);
    // Leave room for the system status bar, as the SDK's browser-minimal example does
    engine.rootContext()->setContextProperty("windowHeight", ScreenHeight() - PanelHeight());
    engine.rootContext()->setContextProperty("bookOrbit", &client);
    engine.rootContext()->setContextProperty("eink", &eink);
    engine.rootContext()->setContextProperty("diagnostics", &diagnosticsLog);
    // Icons from the system theme: image://theme/<name>
    engine.addImageProvider("theme", new ThemeImageProvider);
    engine.load(QUrl("qrc:/qml/main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    diagnostics::start(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));
    diagnostics::log(QString("screen %1x%2, panel height %3, dpi %4, scale %5")
                         .arg(ScreenWidth()).arg(ScreenHeight()).arg(PanelHeight())
                         .arg(get_screen_dpi()).arg(get_screen_scale_factor()));
    diagnostics::log(QString("font %1, Qt using %2")
                         .arg(QString::fromUtf8(iv_get_default_font(FONT_FAMILY)), QFontInfo(app.font()).family()));
    client.start();
    return app.exec();
}
