TARGET = pbookorbit.app
QT += quick network
CONFIG += c++17 qtquickcompiler

SOURCES += main.cpp accents.cpp bookfinder.cpp bookorbitclient.cpp booklistmodel.cpp collectionicons.cpp covers.cpp database.cpp downloads.cpp einkcolors.cpp diagnostics.cpp eink.cpp invertedimageprovider.cpp themeimageprovider.cpp
HEADERS += accents.h bookfinder.h bookorbitclient.h booklistmodel.h collectionicons.h covers.h database.h downloads.h einkcolors.h formats.h diagnostics.h eink.h invertedimageprovider.h themeimageprovider.h
RESOURCES += qml.qrc

# qmake SUGGESTED_SERVER=books.example.com: the server field starts filled in with it, for
# a build to hand to someone who uses that server. It goes in a generated header, written
# only when it changes, so a build with another (or none) recompiles what uses it.
SERVER_HEADER = $$OUT_PWD/suggestedserver.h
SERVER_DEFINE = "$${LITERAL_HASH}define SUGGESTED_SERVER \"$$SUGGESTED_SERVER\""
!equals(SERVER_DEFINE, $$cat($$SERVER_HEADER, blob)): write_file($$SERVER_HEADER, SERVER_DEFINE)
!isEmpty(SUGGESTED_SERVER): message("$$TARGET: suggesting the server $$SUGGESTED_SERVER")

include($$(PB_KIT)/qmake/pocketbook.pri)
LIBS += -lsqlite3
