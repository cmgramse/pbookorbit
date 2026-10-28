#include <inkview.h>
#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStyleHints>

#include "core/bookfinder.h"
#include "core/bookorbitclient.h"
#include "core/browser.h"
#include "core/database.h"
#include "core/diagnostics.h"
#include "ui/covers.h"
#include "core/downloads.h"
#include "ui/eink.h"
#include "ui/invertedimageprovider.h"
#include "ui/launchertile.h"
#include "ui/themeimageprovider.h"

int main(int argc, char *argv[])
{
    // Connect to the PocketBook system (opens the emulator window on PC)
    InitInkview(ACTIVETASK);

    // Render Qt through the PocketBook e-ink platform plugin
    setenv("QT_QPA_PLATFORM", "pocketbook2", true);

    QGuiApplication app(argc, argv);
    diagnostics::captureQtMessages();
    app.setFont(QFont(QString::fromUtf8(iv_get_default_font(FONT_FAMILY))));
    // A blinking cursor redraws the screen twice a second while a text field has focus, and
    // every redraw is a slow e-ink update. Keep it solid.
    app.styleHints()->setCursorFlashTime(0);
    LauncherTile launcherTile;

    // /mnt/ext1/system/config on the device, ./system/config in the emulator. PBOOKORBIT_DB
    // lets tests use their own database and leave the saved login alone.
    Database database(qEnvironmentVariable("PBOOKORBIT_DB", QString::fromUtf8(CONFIGPATH "/pbookorbit.db")));
    // First, before anything logs: the diagnostics setting lives in the database
    diagnostics::Log diagnosticsLog(&database);
    BookOrbitClient client(&database);

    // /mnt/ext1/system/cache on the device, ./system/cache in the emulator
    Covers covers(&client, QString::fromUtf8(USERDATA "/cache/pbookorbit-covers"));
    Downloads downloads(&client, &database);
    BookFinder bookFinder(&client, &downloads, &database);
    // Saved with the book, for offline
    QObject::connect(&downloads, &Downloads::downloaded, &client, &BookOrbitClient::loadBookDetails);
    Browser browser;
    Eink eink;
    eink.followNightMode();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("screenWidth", ScreenWidth());
    // Pixels per dp for units.js, scaled the same way as the SDK's qml_test example
    engine.rootContext()->setContextProperty("dpScale", get_screen_dpi() * get_screen_scale_factor() / 160.0);
    // Leave room for the system status bar, as the SDK's browser-minimal example does
    engine.rootContext()->setContextProperty("windowHeight", ScreenHeight() - PanelHeight());
    engine.rootContext()->setContextProperty("bookOrbit", &client);
    engine.rootContext()->setContextProperty("covers", &covers);
    engine.rootContext()->setContextProperty("downloads", &downloads);
    engine.rootContext()->setContextProperty("bookFinder", &bookFinder);
    engine.rootContext()->setContextProperty("browser", &browser);
    engine.rootContext()->setContextProperty("eink", &eink);
    engine.rootContext()->setContextProperty("diagnostics", &diagnosticsLog);
    engine.rootContext()->setContextProperty("launcherTile", &launcherTile);
    // Icons from the system theme: image://theme/<name>
    engine.addImageProvider("theme", new ThemeImageProvider);
    // Images drawn inverted for night mode: image://inverted/<file or qrc URL>
    engine.addImageProvider("inverted", new InvertedImageProvider);
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
