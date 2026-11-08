# The real client against a loopback fake server
TEMPLATE = app
TARGET = client-tests
QT += core gui network quick testlib
CONFIG += c++17 console testcase no_testcase_installs
CONFIG -= app_bundle
DEFINES += EMULATOR
# Each build has its own scratch folder, so suites built separately don't disturb each other
DEFINES += FLASHDIR=\\\"$$OUT_PWD/scratch\\\"

INCLUDEPATH += ../../src ../stubs ..
LIBS += -lsqlite3

HEADERS += \
    ../fakeserver.h \
    ../../src/core/bookfinder.h \
    ../../src/core/booklistmodel.h \
    ../../src/core/bookorbitclient.h \
    ../../src/core/diagnostics.h \
    ../../src/core/downloads.h \
    ../../src/core/libraryselection.h
SOURCES += \
    tst_library.cpp \
    ../../src/core/bookfinder.cpp \
    ../../src/core/booklistmodel.cpp \
    ../../src/core/bookorbitclient.cpp \
    ../../src/core/database.cpp \
    ../../src/core/diagnostics.cpp \
    ../../src/core/downloads.cpp \
    ../../src/core/libraryselection.cpp \
    ../../src/ui/accents.cpp \
    ../../src/ui/einkcolors.cpp
