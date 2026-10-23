import QtQuick
import QtQuick.Controls
import "units.js" as Units

// Full-window search page, after the library app's search on an Era Color: the toolbar
// becomes a back button and a search box, with recent searches below.
Rectangle {
    id: page
    color: "white"

    // Same height as the main toolbar, so the page below doesn't jump when switching
    property int headerHeight: Units.dp(56)
    // Recent search terms, newest first
    property var recentSearches: []

    // True while the search box has focus, for the quicker e-ink update mode
    readonly property bool typing: searchField.activeFocus

    signal back()
    signal search(string text)

    // Focus the search box, which brings up the on-screen keyboard
    function open() {
        searchField.forceActiveFocus()
    }

    function close() {
        page.forceActiveFocus()
        Qt.inputMethod.hide()
        searchField.text = ""
        page.back()
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
            TextField {
                id: searchField
                anchors.left: searchIcon.status === Image.Ready ? searchIcon.right : parent.left
                anchors.right: parent.right
                anchors.margins: searchBox.thickBorder
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: Units.dp(3)
                font.pointSize: 9
                placeholderText: "Search…"
                placeholderTextColor: "#949494"
                background: null
                onAccepted: if (text.trim() !== "") page.search(text.trim())
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

    // "RECENT SEARCH" heading over a full-width line
    Item {
        id: recentHeading
        anchors.top: header.bottom
        width: parent.width
        height: Units.dp(56)   // 122px

        Text {
            anchors.centerIn: parent
            text: "Recent search"
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
        anchors.top: recentHeading.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: page.recentSearches

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
                    searchField.text = modelData
                    page.search(modelData)
                }
            }
        }
    }
}
