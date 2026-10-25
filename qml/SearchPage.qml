import QtQuick
import QtQuick.Controls
import "units.js" as Units

// Full-window search page, after the library app's search on an Era Color: the toolbar
// becomes a back button and a search box, with recent searches below. From three
// characters on, the books matching the search replace the recent searches.
//
// A search is kept in the recent searches once the keyboard goes away for it: Enter,
// scrolling the results or tapping a book (or the keyboard's own close key). Going back
// doesn't keep it.
Rectangle {
    id: page
    color: "white"

    // Same height as the main toolbar, so the page below doesn't jump when switching
    property int headerHeight: Units.dp(56)
    // True while the search box has focus, for the quicker e-ink update mode
    readonly property bool typing: searchField.activeFocus
    // Enough typed to search (bookOrbit.search() ignores anything shorter)
    readonly property bool showResults: searchField.text.trim().length >= 3

    signal back()
    signal bookClicked(var book)
    signal bookHeld(BookRow row)

    // Set while going back, so the search box losing focus doesn't keep the search
    property bool closing: false

    // Focus the search box, which brings up the on-screen keyboard
    function open() {
        searchField.forceActiveFocus()
    }

    function close() {
        closing = true
        hideKeyboard()
        searchField.text = ""
        closing = false
        page.back()
    }

    // Ends the typing: the search box loses focus, which keeps the search
    function hideKeyboard() {
        page.forceActiveFocus()
        Qt.inputMethod.hide()
    }

    // Closing the on-screen keyboard ends the editing: the box loses focus and its thick
    // border. Tapping the box focuses it again.
    Connections {
        target: Qt.inputMethod
        enabled: page.visible
        function onVisibleChanged() {
            if (!Qt.inputMethod.visible && searchField.activeFocus)
                page.forceActiveFocus()
        }
    }

    Item {
        id: header
        width: parent.width
        height: page.headerHeight

        IconButton {
            id: backButton
            // Chevron with a little space either side; the search box starts after it
            x: Units.dp(7)   // 15px
            anchors.verticalCenter: parent.verticalCenter
            icon: "back"
            pressedIcon: "back_inv"
            fallbackText: "Back"
            onClicked: page.close()
        }

        // Fills the toolbar right of the back button. While the field has focus it gets a
        // thick black border (covering the toolbar's bottom line); otherwise just a 1px line
        // on its left, like the rest of the toolbar.
        Rectangle {
            id: searchBox
            readonly property int thickBorder: Units.dp(2)   // 5px
            x: backButton.x + backButton.width + Units.dp(7)
            width: parent.width - x
            height: parent.height
            border.width: searchField.activeFocus ? thickBorder : 0
            border.color: "black"

            Rectangle {
                visible: !searchField.activeFocus
                width: 1
                height: parent.height
                color: "black"
            }

            Image {
                id: searchIcon
                anchors.left: parent.left
                anchors.leftMargin: searchBox.thickBorder
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/search_transp"
                // Not every theme has the transparent version
                onStatusChanged: if (status === Image.Error) source = "image://theme/search"
            }
            // Clears the box, once there is something in it. Under three characters the
            // results give way to the recent searches, so clearing goes back to them.
            IconButton {
                id: clearButton
                visible: searchField.text !== ""
                anchors.right: parent.right
                anchors.rightMargin: searchBox.thickBorder
                anchors.verticalCenter: parent.verticalCenter
                // The icon at 2/3 its own size (110px), the tap target still the full size
                width: Units.dp(51)        // 111px
                height: Units.dp(51)
                iconSize: Units.dp(34)     // 74px
                icon: "remove"
                pressedIcon: "remove_inv"
                fallbackText: "×"
                onClicked: searchField.text = ""
            }
            TextField {
                id: searchField
                anchors.left: searchIcon.status === Image.Ready ? searchIcon.right : parent.left
                anchors.right: clearButton.visible ? clearButton.left : parent.right
                anchors.margins: searchBox.thickBorder
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: Units.dp(3)
                font.pointSize: 9
                placeholderText: "Search…"
                placeholderTextColor: "#949494"
                background: null
                // Search once typing pauses, rather than for every letter; clearing the
                // results (under three characters) needn't wait
                onTextChanged: {
                    // The text itself: showResults may not have caught up with it yet
                    if (text.trim().length >= 3)
                        searchTimer.restart()
                    else {
                        searchTimer.stop()
                        bookOrbit.search("")
                    }
                }
                onAccepted: {
                    if (text.trim() === "")
                        return
                    searchTimer.stop()
                    bookOrbit.search(text)
                    // Done typing: put the keyboard away to show the results
                    page.hideKeyboard()
                }
                // However the typing ends, bar going back, the search is kept
                onActiveFocusChanged: {
                    if (!activeFocus && page.visible && !page.closing)
                        bookOrbit.addRecentSearch(text)
                }
            }
            Timer {
                id: searchTimer
                interval: 400
                onTriggered: bookOrbit.search(searchField.text)
            }
        }

        // Toolbar line along the bottom, under the back button too
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: "black"
        }
    }

    // "RECENT SEARCHES" heading over a full-width line
    Item {
        id: recentHeading
        visible: !page.showResults
        anchors.top: header.bottom
        width: parent.width
        height: Units.dp(56)   // 122px

        Text {
            anchors.centerIn: parent
            text: "Recent searches"
            color: "#737373"
            font.pointSize: 9
            font.capitalization: Font.AllUppercase
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: "black"
        }
    }

    ListView {
        visible: !page.showResults
        anchors.top: recentHeading.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: bookOrbit.recentSearches

        delegate: Item {
            width: ListView.view.width
            height: Units.dp(56)

            Rectangle {
                anchors.fill: parent
                color: "black"
                visible: rowMouse.pressed
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: Units.dp(42)   // 92px
                anchors.right: parent.right
                anchors.rightMargin: Units.dp(26)
                anchors.verticalCenter: parent.verticalCenter
                text: modelData
                color: rowMouse.pressed ? "white" : "black"
                font.pointSize: 9
                elide: Text.ElideRight
            }
            // 1px grey line, inset from both sides
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Units.dp(26)   // 57px
                anchors.rightMargin: Units.dp(26)
                height: 1
                color: "#7f7f7f"
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                onClicked: {
                    // Searched again, so it moves to the top. The list changes under the
                    // tap, so take the text first.
                    const search = modelData
                    searchField.text = search
                    bookOrbit.addRecentSearch(search)
                }
            }
        }
    }

    // Matching books, in the library's rows without its sections. Loads as it scrolls,
    // like the library list.
    ListView {
        id: results
        visible: page.showResults
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: bookOrbit.searchResults

        function loadVisibleBooks() {
            if (count === 0)
                return
            let first = indexAt(width / 2, contentY)
            let last = indexAt(width / 2, contentY + height - 1)
            if (first < 0)
                first = 0
            if (last < 0)
                last = count - 1
            const screenful = last - first + 1
            bookOrbit.loadSearchResults(first - screenful, last + screenful)
        }
        Timer {
            id: loadTimer
            interval: 150
            onTriggered: results.loadVisibleBooks()
        }
        onContentYChanged: loadTimer.restart()
        onCountChanged: loadTimer.restart()

        // New results start at the top
        Connections {
            target: bookOrbit
            function onSearchChanged() {
                if (bookOrbit.searchTotal === -1)
                    results.positionViewAtBeginning()
            }
        }

        // Fast black-and-white updates while it scrolls, as in the library list. Scrolling
        // the results ends the typing.
        onMovementStarted: {
            page.hideKeyboard()
            const pos = mapToGlobal(0, 0)
            eink.enterA2ForFlick(pos.x, pos.y, width, height)
        }
        onMovementEnded: eink.leaveA2Soon()
        onVisibleChanged: if (!visible) eink.leaveA2()

        delegate: BookRow {
            id: bookRow
            required book
            // Ends the typing too, which keeps the search
            onClicked: {
                page.hideKeyboard()
                page.bookClicked(book)
            }
            onHeld: {
                page.hideKeyboard()
                page.bookHeld(bookRow)
            }
        }
    }

    ListScrollbar {
        list: results
    }

    // While the first results load, or when nothing matches
    Text {
        visible: page.showResults && results.count === 0
        anchors.top: header.bottom
        anchors.topMargin: Units.dp(26)
        anchors.horizontalCenter: parent.horizontalCenter
        text: bookOrbit.searchTotal === 0 ? "No books found" : "Searching…"
        color: "#737373"
        font.pointSize: 9
    }
}
