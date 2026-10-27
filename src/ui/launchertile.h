#pragma once

#include <QObject>

// Adds a PBookOrbit tile with its own icon to the PocketBook home screen (src/icons:
// 110x110 8-bit grey, the Era Color's theme icon size). The launcher reads its tiles from
// system/config/desktop/view.json on user storage, so the app adds itself there on each
// start. The launcher only reads the file at boot, so the tile appears after a restart. The
// task manager gets the same name and icon. See NOTES.md.
class LauncherTile : public QObject
{
    Q_OBJECT
    // Whether this start added the tile, so the user should be told to restart
    Q_PROPERTY(bool added READ added CONSTANT)

public:
    // Writes the icons to applications/icons unless already there (the user may have
    // replaced them), and adds or fixes the app's view.json entry. Then gives the running
    // task the same name and icon. Failures are logged and skipped so the app still starts.
    // The emulator has no view.json, so there it only writes the icons.
    explicit LauncherTile(QObject *parent = nullptr);
    bool added() const { return m_added; }

    // Restarts the device so the launcher shows the tile. Only logs in the emulator.
    Q_INVOKABLE void restartDevice();

private:
    bool m_added = false;
};
