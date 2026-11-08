# The selector's QML, with Qt Quick Test
TEMPLATE = app
TARGET = qml-tests
QT += qml quick qmltest
CONFIG += c++17 console qmltestcase
CONFIG -= app_bundle
SOURCES += main.cpp
# The app's icons, which main.qml refers to
RESOURCES += ../../src/qml.qrc
DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\"
DEFINES += QML_SOURCE_DIR=\\\"$$PWD/../../src/qml\\\"
