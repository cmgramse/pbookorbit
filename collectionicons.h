#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

class BookOrbitClient;

// Collection icons as SVG files, which QML's Image draws. A BookOrbit collection's icon is
// either the name of a Lucide icon, as its web client's @lucide/vue exports them
// ("FolderOpen"), or "custom:<slug>" for one uploaded to the server.
//
// Lucide icons are made from the copy of the icon set in lucide/ (see update.py there),
// custom ones downloaded (GET /custom-icons/<slug>.svg, fetched again once a run in case
// it changed). Both are drawn in black and kept in the cache folder.
//
// QML asks for an icon with source(); if a custom one isn't cached yet the result is
// empty, a download starts, and ready(icon) follows once source() has it.
class CollectionIcons : public QObject
{
    Q_OBJECT

public:
    CollectionIcons(BookOrbitClient *client, const QString &cacheDir, QObject *parent = nullptr);

    // File URL of the icon's SVG, or "" while it downloads. An unknown Lucide name gets
    // BookOrbit's fallback, a folder.
    Q_INVOKABLE QString source(const QString &icon);

signals:
    void ready(const QString &icon);

private:
    // The SVG file for a Lucide icon, written if it isn't there yet; "" if there is no
    // such icon
    QString lucideFile(const QString &name);
    void loadLucide();

    BookOrbitClient *m_client;
    QString m_cacheDir;
    // Kebab-case name -> [[tag, attrs], ...], and PascalCase names -> kebab-case
    QJsonObject m_lucideIcons;
    QHash<QString, QString> m_lucideNames;
    bool m_lucideLoaded = false;
    // Custom icons downloaded this run, or downloading
    QSet<QString> m_fetched;
};
