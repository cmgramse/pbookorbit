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

contains(PLATFORM, arm) {
    # Cross-compile for the device (B288: armv7, Qt6 from the SDK sysroot)
    SYSROOT = $$(PB_SYSROOT)
    INCLUDEPATH += $$SYSROOT/usr/include $$SYSROOT/usr/local/include $$SYSROOT/usr/include/freetype2
    message("$$TARGET: building for ARM, sysroot $$SYSROOT")
} else {
    # Desktop build: libinkview's emulator draws the "device screen" in an X11 window
    CONFIG += link_pkgconfig
    PKGCONFIG += freetype2
    DEFINES += EMULATOR=1 PB_LOG_LEVEL=1
    DEFINES += "EMULATOR_SDK_PATH=\\\"$$(PB_SDK_DIR)/usr/local\\\""
    LIBS += -L$$(PB_SDK_DIR)/usr/lib -L$$(PB_SDK_DIR)/usr/local/lib
    INCLUDEPATH += $$(PB_SDK_DIR)/usr/local/include $$(PB_SDK_DIR)/usr/include
    QMAKE_RPATHDIR += $$(PB_SDK_DIR)/usr/local/lib
    message("$$TARGET: building for PC emulator, SDK $$(PB_SDK_DIR)")
    system(echo [Paths] > $$OUT_PWD/qt.conf; echo Prefix = $$(PB_SDK_DIR)/usr/local/qt6 >> $$OUT_PWD/qt.conf)
}

LIBS += -ldl -lz -lsqlite3 -lhwconfig -linkview -lcommon_utilities
