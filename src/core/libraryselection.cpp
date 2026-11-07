#include "core/libraryselection.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

bool parseLibraries(const QJsonDocument &doc, QList<LibraryChoice> *libraries)
{
    libraries->clear();
    if (!doc.isArray())
        return false;
    const QJsonArray array = doc.array();
    QSet<int> ids;
    QHash<QString, int> nameCounts;
    for (const QJsonValue &value : array) {
        const QJsonObject library = value.toObject();
        // toInt() gives the default for anything but a whole number
        const int id = library.value("id").toInt(0);
        if (id <= 0 || ids.contains(id))
            continue;
        ids.insert(id);
        const QString name = library.value("name").toString();
        nameCounts[name]++;
        libraries->append({id, name, name, library.value("bookCount").toInt()});
    }
    for (LibraryChoice &library : *libraries) {
        if (library.name.isEmpty())
            library.label = QString("Library %1").arg(library.id);
        else if (nameCounts.value(library.name) > 1)
            library.label = QString("%1 #%2").arg(library.name).arg(library.id);
    }
    return array.isEmpty() || !libraries->isEmpty();
}

int chooseLibrary(const QList<LibraryChoice> &libraries, int saved)
{
    for (const LibraryChoice &library : libraries) {
        if (library.id == saved)
            return saved;
    }
    return libraries.isEmpty() ? -1 : libraries.first().id;
}

QString libraryPreferenceKey(const QString &serverUrl, const QString &username)
{
    return QString("selectedLibrary|%1|%2").arg(serverUrl, username.toLower());
}
