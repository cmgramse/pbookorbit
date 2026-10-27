#pragma once

#include <QQuickImageProvider>

// Serves icons from the device's system theme to QML so controls match the built-in apps,
// e.g. "image://theme/home" or "image://theme/list_menu_inv".
//
// Names are the theme's resource names, which vary by device and firmware; pocketbook-kit's
// pb device import saves the full list from a device. Most icons have an "_inv" pressed
// version. For those that don't, "<name>@inverted" returns the icon with inverted colours.
//
// Based on ResourceImageProvider in the SDK's qml_test example.
class ThemeImageProvider : public QQuickImageProvider
{
public:
    ThemeImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
