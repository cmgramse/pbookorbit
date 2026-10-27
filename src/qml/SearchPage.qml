import QtQuick
import QtQuick.Controls
import "units.js" as Units

// Full-window search page, modelled on the Era Color library's search: the toolbar turns
// into a back button and search box, with recent searches below. From three characters,
// matching books replace the recent searches.
//
// A search is saved to recent searches when the keyboard closes for it (Enter, scrolling
// the results, tapping a book, or the keyboard's close key). Going back doesn't save it.
Rectangle {
    id: page
    // For the page-turn keys, which main.qml forwards to the front page
    readonly property alias pageButtons: resultsPages
    color: "white"

    // Same height as the main toolbar, so the page below doesn't jump
    property int headerHeight: Units.dp(56)
    // True while the search box has focus, for faster e-ink updates
    readonly property bool typing: searchField.activeFocus
    // Long enough to search (bookOrbit.search() ignores shorter text)
    readonly property bool showResults: searchField.text.trim().length >= 3

    signal back()
    signal bookClicked(var book)
    signal bookHeld(BookRow row)

    // Set while going back, so losing focus doesn't save the search
    property bool closing: false

    // Focusing the search box opens the on-screen keyboard
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

    // Finishes typing: the box loses focus, which saves the search
    function hideKeyboard() {
        page.forceActiveFocus()
        Qt.inputMethod.hide()
    }

    // Closing the on-screen keyboard ends editing: the box loses focus and its thick
    // border. Tapping it focuses it again.
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
            x: Units.dp(7)
            anchors.verticalCenter: parent.verticalCenter
            icon: "back"
            pressedIcon: "back_inv"
            fallbackText: "Back"
            onClicked: page.close()
        }

        // Fills the toolbar right of the back button. With focus it gets a thick black
        // border (covering the toolbar's bottom line), otherwise a 1px line on its left
        // like the rest of the toolbar.
        Rectangle {
            id: searchBox
            readonly property int thickBorder: Units.dp(2)
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
            // Clears the box once it has text. Under three characters the recent searches
            // come back, so clearing returns to them.
            IconButton {
                id: clearButton
                visible: searchField.text !== ""
                anchors.right: parent.right
                anchors.rightMargin: searchBox.thickBorder
                anchors.verticalCenter: parent.verticalCenter
                // Icon at 2/3 of its natural size, tap target full size
                width: Units.dp(51)
                height: Units.dp(51)
                iconSize: Units.dp(34)
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
                font.pointSize: 8
                placeholderText: "Search…"
                placeholderTextColor: "#949494"
                background: null
                // Search when typing pauses, not on every letter. Clearing (under three
                // characters) needn't wait.
                onTextChanged: {
                    // Use the text itself; showResults may not have caught up yet
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
                    // Done typing: hide the keyboard to show the results
                    page.hideKeyboard()
                }
                // Save the search however typing ends, except going back
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

        // Toolbar bottom line, under the back button too
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
        height: Units.dp(56)

        Text {
            anchors.centerIn: parent
            text: "Recent searches"
            color: "#737373"
            font.pointSize: 8
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
                anchors.leftMargin: Units.dp(42)
                anchors.right: parent.right
                anchors.rightMargin: Units.dp(26)
                anchors.verticalCenter: parent.verticalCenter
                text: modelData
                color: rowMouse.pressed ? "white" : "black"
                font.pointSize: 8
                elide: Text.ElideRight
            }
            // 1px grey line, inset from both sides
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Units.dp(26)
                anchors.rightMargin: Units.dp(26)
                height: 1
                color: "#7f7f7f"
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                onClicked: {
                    // Searched again, so it moves to the top. Grab the text first, since
                    // the list changes under the tap.
                    const search = modelData
                    searchField.text = search
                    bookOrbit.addRecentSearch(search)
                }
            }
        }
    }

    // Matching books, as library rows without sections. Loads on scroll like the library
    // list.
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

        // Fast black-and-white updates while scrolling, as in the library list. Scrolling
        // also ends typing.
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
            // Also ends typing, which saves the search
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
        bottomInset: resultsPages.height
    }
    PageButtons {
        id: resultsPages
        list: results
        anchors.bottom: results.bottom
    }

    // While the first results load
    Spinner {
        list: results
        visible: page.showResults && results.count === 0 && bookOrbit.searchTotal !== 0
    }
    // When nothing matches
    Text {
        visible: page.showResults && results.count === 0 && bookOrbit.searchTotal === 0
        anchors.top: header.bottom
        anchors.topMargin: Units.dp(26)
        anchors.horizontalCenter: parent.horizontalCenter
        text: "No books found"
        color: "#737373"
        font.pointSize: 8
    }
}
