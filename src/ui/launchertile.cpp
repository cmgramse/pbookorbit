#include "ui/launchertile.h"

#include "core/diagnostics.h"

#include <inkview.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <unistd.h>

namespace {

const QString TITLE = "PBookOrbit";
// The app's key in view.json's "applications" and in its groups' "apps" lists. The
// firmware's comment in the file: user apps' keys must start with "U_".
const QString ENTRY = "U_pbookorbit";

const QString VIEW_FILE = CONFIGPATH "/desktop/view.json";
// view.json as it was before the app first changed it, kept for putting things back
const QString VIEW_BACKUP = CONFIGPATH "/desktop/view.json.before-pbookorbit";
// Where the user can find them to change them
const QString ICON_DIR = FLASHDIR "/applications/icons";
const QString ICON_FILE = ICON_DIR + "/pbookorbit.bmp";
const QString FOCUSED_ICON_FILE = ICON_DIR + "/pbookorbit_f.bmp";

// Saved whole or not at all: the launcher shows no picture for a broken icon, and might
// not start with a broken view.json
bool save(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit())
        return true;
    diagnostics::log("launcher tile: can't write " + path + ": " + file.errorString());
    return false;
}

// Only when it isn't there: the user may have drawn their own
bool saveIfMissing(const QString &path, const QString &resource)
{
    if (QFile::exists(path))
        return true;
    QFile source(resource);
    return source.open(QIODevice::ReadOnly) && save(path, source.readAll());
}

// Paths in view.json: the app's absolute, the icons as "object paths", relative to the
// storage's root (the firmware's comment in the file explains both)
QJsonObject launcherEntry()
{
    // Absolute first: the emulator's paths are relative to where it runs
    const QDir storage(QDir(FLASHDIR).absolutePath());
    const auto fromStorage = [&](const QString &file) {
        return QJsonObject{{"path", storage.relativeFilePath(QDir(file).absolutePath())}};
    };
    return {
        {"title", TITLE},
        {"path", QCoreApplication::applicationFilePath()},
        {"icon", fromStorage(ICON_FILE)},
        {"focused_icon", fromStorage(FOCUSED_ICON_FILE)},
    };
}

// True if this added the tile, rather than finding it there or only updating it
bool addToView()
{
    QFile file(VIEW_FILE);
    if (!file.open(QIODevice::ReadOnly)) {
        diagnostics::log("launcher tile: no " + VIEW_FILE);
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (!document.isObject()) {
        diagnostics::log("launcher tile: can't read " + VIEW_FILE + ": " + error.errorString());
        return false;
    }

    QJsonObject view = document.object();
    QJsonObject apps = view.value("applications").toObject();
    const bool added = !apps.contains(ENTRY);
    // Our keys over whatever else the entry has, which the launcher may have added
    const QJsonObject before = apps.value(ENTRY).toObject();
    QJsonObject entry = before;
    const QJsonObject wanted = launcherEntry();
    for (auto it = wanted.begin(); it != wanted.end(); ++it)
        entry.insert(it.key(), it.value());
    if (!added && entry == before)
        return false;
    apps.insert(ENTRY, entry);
    view.insert("applications", apps);

    // A new entry goes at the end of the first group of tiles. Later starts leave the
    // groups alone, so a tile the user moved or took off stays where they put it.
    if (added) {
        QJsonObject layout = view.value("view").toObject();
        QJsonArray groups = layout.value("groups").toArray();
        if (groups.isEmpty()) {
            diagnostics::log("launcher tile: view.json has no groups to add the tile to");
        } else {
            QJsonObject first = groups.first().toObject();
            QJsonArray tiles = first.value("apps").toArray();
            tiles.append(ENTRY);
            first.insert("apps", tiles);
            groups.replace(0, first);
            layout.insert("groups", groups);
            view.insert("view", layout);
        }
        if (!QFile::exists(VIEW_BACKUP))
            QFile::copy(VIEW_FILE, VIEW_BACKUP);
    }

    if (!save(VIEW_FILE, QJsonDocument(view).toJson()))
        return false;
    diagnostics::log(added ? "launcher tile: added to view.json" : "launcher tile: updated in view.json");
    return added;
}

}

LauncherTile::LauncherTile(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(ICON_DIR);
    if (saveIfMissing(ICON_FILE, ":/icons/pbookorbit.bmp")
            && saveIfMissing(FOCUSED_ICON_FILE, ":/icons/pbookorbit_f.bmp"))
        m_added = addToView();
}

void LauncherTile::restartDevice()
{
#ifdef EMULATOR
    diagnostics::log("would restart the device");
#else
    // As KOReader does on PocketBook. Written out first: the restart may not wait for it.
    diagnostics::log("restarting the device");
    sync();
    iv_ipc_request(MSG_REBOOT, 1, nullptr, 0, 0);
#endif
}
