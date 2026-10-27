#pragma once

#include <QQuickImageProvider>

// Serves icons from the device's system theme to QML, so the app's controls match the
// built-in apps: "image://theme/home", "image://theme/list_menu_inv" and so on.
//
// The names are the theme's resource names. Themes differ between devices and firmware:
// pocketbook-kit's pb device import saves the full list from a device. Most icons have an
// "_inv" version for the pressed state; for those that don't, "<name>@inverted" returns
// the icon with its colours inverted (white on black).
//
// Based on ResourceImageProvider in the SDK's qml_test example.
class ThemeImageProvider : public QQuickImageProvider
{
public:
    ThemeImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
