#pragma once

#include <QObject>

// Gives the app its own tile on the PocketBook home screen, named PBookOrbit and with its
// own icon (src/icons: 110x110 8-bit grey, the size of the Era Color's theme icons). The
// launcher lists its tiles in system/config/desktop/view.json, which only the user's
// storage holds, so the app adds itself there each time it starts. The launcher reads it
// at boot: the tile appears after the device restarts. The same name and icon go to the
// task manager. See NOTES.md.
class LauncherTile : public QObject
{
    Q_OBJECT
    // Whether this start added the tile, so the user should be told to restart
    Q_PROPERTY(bool added READ added CONSTANT)

public:
    // Writes the icons to applications/icons, unless they are there already (the user may
    // have changed them), and adds the app to view.json if it isn't there as it should be.
    // Then gives the running task the same name and icon, for the task manager.
    // Anything that fails is logged and skipped, so the app starts anyway. The emulator
    // has no view.json: there it only writes the icons.
    explicit LauncherTile(QObject *parent = nullptr);
    bool added() const { return m_added; }

    // Restarts the device, for the launcher to show the tile. Only logs in the emulator.
    Q_INVOKABLE void restartDevice();

private:
    bool m_added = false;
};
