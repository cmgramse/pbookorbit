TARGET = bookorbit.app
QT += quick network
CONFIG += c++17 qtquickcompiler

SOURCES += main.cpp bookorbitclient.cpp diagnostics.cpp eink.cpp
HEADERS += bookorbitclient.h diagnostics.h eink.h
RESOURCES += qml.qrc

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

LIBS += -ldl -lz -lhwconfig -linkview -lcommon_utilities
