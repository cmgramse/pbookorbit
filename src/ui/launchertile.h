#pragma once

#include <QObject>

// Gives the app its own tile on the PocketBook home screen, named PBookOrbit and with its
// own icon (src/icons: 106x110 8-bit grey, the size of tiles on the Era Color). The
// launcher lists its tiles in system/config/desktop/view.json, which only the user's
// storage holds, so the app adds itself there each time it starts. The launcher reads it
// at boot: the tile appears after the device restarts. See NOTES.md.
class LauncherTile : public QObject
{
    Q_OBJECT
    // Whether this start added the tile, so the user should be told to restart
    Q_PROPERTY(bool added READ added CONSTANT)

public:
    // Writes the icons to applications/icons, unless they are there already (the user may
    // have changed them), and adds the app to view.json if it isn't there as it should be.
    // Anything that fails is logged and skipped, so the app starts anyway. The emulator
    // has no view.json: there it only writes the icons.
    explicit LauncherTile(QObject *parent = nullptr);
    bool added() const { return m_added; }

    // Restarts the device, for the launcher to show the tile. Only logs in the emulator.
    Q_INVOKABLE void restartDevice();

private:
    bool m_added = false;
};
