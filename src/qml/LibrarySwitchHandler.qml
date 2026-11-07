import QtQuick

// What the rest of the app does when the library changes, apart from main.qml so it can be
// tested
Item {
    id: handler
    property var client
    property bool showingSeries: false
    property bool showingAuthors: false
    signal closeAll()
    signal scrollToTop()
    signal reloadSeries()
    signal reloadAuthors()
    signal fallback(string label)

    Connections {
        target: handler.client
        function onLibrarySwitched() {
            handler.closeAll()
            handler.scrollToTop()
            // A tab already showing won't load itself. With no library left, there's nothing
            // to load.
            if (handler.client.currentLibraryId > 0) {
                if (handler.showingSeries)
                    handler.reloadSeries()
                if (handler.showingAuthors)
                    handler.reloadAuthors()
            }
        }
        function onLibraryFallback() {
            handler.fallback(handler.client.currentLibraryLabel)
        }
    }
}
