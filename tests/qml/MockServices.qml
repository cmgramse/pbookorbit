import QtQuick

// Stands in for the C++ objects main.cpp gives the app, so main.qml's wiring can run. Only
// what the tests look at has a value.
QtObject {
    property bool signedIn: true
    property bool online: true
    property bool busy: false
    property bool booksLoaded: true
    property string error: ""
    property var libraries: [{id: 1, label: "Alpha"}, {id: 2, label: "Beta"}]
    property int currentLibraryId: 1
    property string currentLibraryLabel: "Alpha"
    signal librarySwitched()
    signal libraryFallback()
}
