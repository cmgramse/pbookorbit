import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import "units.js" as Units

Window {
    id: root
    visible: true
    width: screenWidth
    height: windowHeight
    color: "white"

    // Quicker e-ink updates while typing. Applied with callLater because moving focus
    // from one field to the other briefly leaves neither focused.
    readonly property bool typing: (loginPage.visible && loginPage.typing)
                                   || (searchPage.visible && searchPage.typing)
    onTypingChanged: Qt.callLater(() => eink.setDynamicUpdates(root.typing))

    // Search page open over everything else
    property bool searching: false

    function openSearch() {
        menu.visible = false
        searching = true
        searchPage.open()
    }

    // The book list is the only page for now, so home just goes back to its top
    function goHome() {
        menu.visible = false
        bookList.positionViewAtBeginning()
    }

    AppHeader {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        title: "PocketBook × BookOrbit"
        menuOpen: menu.visible
        showSearch: bookOrbit.signedIn
        onHomeClicked: root.goHome()
        onSearchClicked: root.openSearch()
        onMenuClicked: menu.visible = !menu.visible
    }

    // Signed out
    LoginPage {
        id: loginPage
        visible: !bookOrbit.signedIn
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
    }

    // Signed in: the home page, with the most recently added books
    ColumnLayout {
        visible: bookOrbit.signedIn
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Units.dp(20)
        spacing: Units.dp(12)
        
        Text {
            visible: bookOrbit.error !== "" || bookOrbit.busy
            text: bookOrbit.busy ? "Working…" : bookOrbit.error
            font.pointSize: 8
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        SectionHeading {
            // No count until the first page has loaded
            text: "Recently added" + (bookOrbit.totalBooks ? " (" + bookOrbit.books.length + " of " + bookOrbit.totalBooks + ")" : "")
        }
        ListView {
            id: bookList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            // No bounce at the ends: on e-ink it is only extra redraws
            boundsBehavior: Flickable.StopAtBounds
            model: bookOrbit.books

            // Fast black-and-white updates for just the list while it scrolls. A2 takes
            // screen coordinates, hence mapToGlobal (as in the SDK's qml_test example).
            onMovementStarted: {
                const pos = mapToGlobal(0, 0)
                eink.enterA2ForFlick(pos.x, pos.y, width, height)
            }
            // Sent once the momentum has stopped, not when the finger lifts
            onMovementEnded: eink.leaveA2()
            onVisibleChanged: if (!visible) eink.leaveA2()
            delegate: Column {
                width: ListView.view.width
                bottomPadding: Units.dp(8)
                Text {
                    width: parent.width
                    text: modelData.title
                    font.pointSize: 9
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: modelData.authors.join(", ")
                          + (modelData.files.length ? "  ·  " + modelData.files.map(f => f.format).join(", ") : "")
                    font.pointSize: 7
                    elide: Text.ElideRight
                }
            }
        }
    }

    // Header menu: drops down from the menu button. As in the system apps, the page
    // below the header fades while it is open; a tap there closes it.
    Rectangle {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        visible: menu.visible
        color: Qt.rgba(1, 1, 1, 0.6)
        MouseArea {
            anchors.fill: parent
            onClicked: menu.visible = false
        }
    }
    PopupMenu {
        id: menu
        visible: false
        anchors.top: header.bottom
        anchors.topMargin: Units.dp(1)
        anchors.right: parent.right
        anchors.rightMargin: Units.dp(9)   // 20px from the screen edge

        PopupMenuItem {
            visible: bookOrbit.signedIn
            text: "Sign out"
            onClicked: {
                menu.visible = false
                root.searching = false
                bookOrbit.signOut()
            }
        }
        PopupMenuItem {
            text: "Exit"
            showSeparator: false
            onClicked: Qt.quit()
        }
    }

    SearchPage {
        id: searchPage
        anchors.fill: parent
        visible: root.searching
        headerHeight: header.height
        // TODO: real recent searches; sample terms until the search logic exists
        recentSearches: ["door", "the faith"]
        onBack: root.searching = false
        // TODO: run the search
        onSearch: (text) => diagnostics.log("search: " + text)
    }
}
