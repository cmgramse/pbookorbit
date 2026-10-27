#include "ui/launchertile.h"

#include "core/diagnostics.h"

#include <inkview.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cstring>
#include <unistd.h>

namespace {

const QString TITLE = "PBookOrbit";
// The app's key in view.json's "applications" and its groups' "apps" lists. The firmware's
// comment in the file says user apps' keys must start with "U_".
const QString ENTRY = "U_pbookorbit";

const QString VIEW_FILE = CONFIGPATH "/desktop/view.json";
// The original view.json, kept so it can be restored
const QString VIEW_BACKUP = CONFIGPATH "/desktop/view.json.before-pbookorbit";
// Somewhere the user can find and replace them
const QString ICON_DIR = FLASHDIR "/applications/icons";
const QString ICON_FILE = ICON_DIR + "/pbookorbit.bmp";
const QString FOCUSED_ICON_FILE = ICON_DIR + "/pbookorbit_f.bmp";

// Written atomically: the launcher shows nothing for a broken icon, and might not start
// with a broken view.json
bool save(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit())
        return true;
    diagnostics::log("launcher tile: can't write " + path + ": " + file.errorString());
    return false;
}

// Only if missing, since the user may have replaced it
bool saveIfMissing(const QString &path, const QString &resource)
{
    if (QFile::exists(path))
        return true;
    QFile source(resource);
    return source.open(QIODevice::ReadOnly) && save(path, source.readAll());
}

// view.json paths: the app's is absolute, the icons are "object paths" relative to the
// storage root (the firmware's comment in the file explains both)
QJsonObject launcherEntry()
{
    // Make absolute first, since the emulator's paths are relative to its working directory
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

// True if this added the tile, as opposed to finding or updating it
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
    // Our keys override the entry's, keeping anything else the launcher added
    const QJsonObject before = apps.value(ENTRY).toObject();
    QJsonObject entry = before;
    const QJsonObject wanted = launcherEntry();
    for (auto it = wanted.begin(); it != wanted.end(); ++it)
        entry.insert(it.key(), it.value());
    if (!added && entry == before)
        return false;
    apps.insert(ENTRY, entry);
    view.insert("applications", apps);

    // New entries go at the end of the first group. Later starts leave the groups alone, so
    // a tile the user moved or removed stays that way.
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

// The tile icon as an 8-bit grey bitmap, the format the task manager expects. InkView's
// LoadBitmap returns 24-bit for a palette BMP, which the task manager draws as three
// squashed copies. Null if the icon can't be read.
const ibitmap *taskIcon()
{
    const QImage image = QImage(ICON_FILE).convertToFormat(QImage::Format_Grayscale8);
    if (image.isNull())
        return nullptr;
    // Never freed, since the task manager may draw from it while the app runs
    ibitmap *icon = NewBitmap8(image.width(), image.height());
    if (!icon)
        return nullptr;
    for (int y = 0; y < image.height(); ++y)
        memcpy(icon->data + y * icon->scanline, image.constScanLine(y), image.width());
    return icon;
}

// The task manager names a task after its program (pbookorbit) unless a language file
// translates it (the firmware has none for user apps), and only shows an icon if the theme
// has one. A task can set both itself.
void nameTask()
{
    const int task = GetCurrentTask();
    // Left unchanged, since OpenTask and FindTaskByAppName look the app up by it
    const taskinfo *info = GetTaskInfo(task);
    const char *appname = info ? info->appname : nullptr;
    const ibitmap *icon = taskIcon();
    const QByteArray title = TITLE.toUtf8();
    const int parameters = SetTaskParameters(task, appname, title.constData(), const_cast<ibitmap *>(icon),
                                             TASK_DONTCHANGE);
    // KOReader names its subtask (the open book) the same way
    const int subtask = SetSubtaskInfo(task, 0, title.constData(), "");
    diagnostics::log(QString("task %1 (%2): named %3, icon %4; SetTaskParameters %5, SetSubtaskInfo %6")
                         .arg(task).arg(QString::fromUtf8(appname ? appname : "no appname"), TITLE,
                              icon ? QString("%1x%2 depth %3").arg(icon->width).arg(icon->height).arg(icon->depth)
                                   : QString("not loaded"))
                         .arg(parameters).arg(subtask));
}

}

LauncherTile::LauncherTile(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(ICON_DIR);
    if (saveIfMissing(ICON_FILE, ":/icons/pbookorbit.bmp")
            && saveIfMissing(FOCUSED_ICON_FILE, ":/icons/pbookorbit_f.bmp"))
        m_added = addToView();
    nameTask();
}

void LauncherTile::restartDevice()
{
#ifdef EMULATOR
    diagnostics::trace("would restart the device");
#else
    // Same as KOReader on PocketBook. Sync first, as the restart may not wait.
    diagnostics::trace("restarting the device");
    sync();
    iv_ipc_request(MSG_REBOOT, 1, nullptr, 0, 0);
#endif
}
