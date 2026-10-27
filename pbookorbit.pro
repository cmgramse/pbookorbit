TARGET = pbookorbit.app
QT += quick network
CONFIG += c++17 qtquickcompiler

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/core/bookfinder.cpp \
    src/core/browser.cpp \
    src/core/booklistmodel.cpp \
    src/core/bookorbitclient.cpp \
    src/core/database.cpp \
    src/core/diagnostics.cpp \
    src/core/downloads.cpp \
    src/ui/accents.cpp \
    src/ui/covers.cpp \
    src/ui/eink.cpp \
    src/ui/einkcolors.cpp \
    src/ui/invertedimageprovider.cpp \
    src/ui/launchertile.cpp \
    src/ui/themeimageprovider.cpp
HEADERS += \
    src/core/bookfinder.h \
    src/core/browser.h \
    src/core/booklistmodel.h \
    src/core/bookorbitclient.h \
    src/core/database.h \
    src/core/diagnostics.h \
    src/core/downloads.h \
    src/core/formats.h \
    src/ui/accents.h \
    src/ui/covers.h \
    src/ui/eink.h \
    src/ui/einkcolors.h \
    src/ui/invertedimageprovider.h \
    src/ui/launchertile.h \
    src/ui/themeimageprovider.h
RESOURCES += src/qml.qrc

# qmake SUGGESTED_SERVER=books.example.com prefills the server field, for builds handed to
# users of that server. It's written to a generated header only when it changes, so
# switching servers recompiles what uses it.
SERVER_HEADER = $$OUT_PWD/suggestedserver.h
SERVER_DEFINE = "$${LITERAL_HASH}define SUGGESTED_SERVER \"$$SUGGESTED_SERVER\""
!equals(SERVER_DEFINE, $$cat($$SERVER_HEADER, blob)): write_file($$SERVER_HEADER, SERVER_DEFINE)
!isEmpty(SUGGESTED_SERVER): message("$$TARGET: suggesting the server $$SUGGESTED_SERVER")

include($$(PB_KIT)/qmake/pocketbook.pri)
LIBS += -lsqlite3
