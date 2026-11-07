#pragma once

#include <QJsonDocument>
#include <QList>
#include <QString>

// The library lists the server offers and which one to use. No device or network code, so
// it can be tested on its own.

struct LibraryChoice {
    int id;
    QString name;
    QString label;
    int bookCount;
};

// True for a JSON array that isn't all bad entries. Keeps server order and drops entries
// whose id isn't a positive integer, and repeated ids. label is the name; "Library <id>" if
// the name is empty; "<name> #<id>" where two entries share a name.
bool parseLibraries(const QJsonDocument &doc, QList<LibraryChoice> *libraries);
// saved if listed, else the first id, else -1
int chooseLibrary(const QList<LibraryChoice> &libraries, int saved);
// The settings key for a server's and user's choice
QString libraryPreferenceKey(const QString &serverUrl, const QString &username);
