import QtQuick
import "units.js" as Units

// Top toolbar: home button, title, optional search button, menu button, and a 1px line
// underneath.
// Follows AppHeader.qml in the SDK's qml_test example: theme icons at their own size,
// dp(15) from the edges. Taller than dp(56) if the theme's icons are.
Rectangle {
    id: header
    property alias title: titleText.text
    property bool menuOpen: false
    property bool showSearch: false
    signal homeClicked()
    signal searchClicked()
    signal menuClicked()

    readonly property int margin: Units.dp(15)
    height: Math.max(Units.dp(56), homeButton.height, menuButton.height) + separator.height
    color: "white"

    IconButton {
        id: homeButton
        anchors.left: parent.left
        anchors.leftMargin: header.margin
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -separator.height / 2
        icon: "home"
        pressedIcon: "home_inv"
        fallbackText: "Home"
        onClicked: header.homeClicked()
    }

    Text {
        id: titleText
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -separator.height / 2
        // Centred on the header, so leave the wider of the two sides' buttons free on both
        width: parent.width - 2 * (header.margin + Math.max(homeButton.width, rightButtons.width))
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        // So part of the title can be coloured: <font color="...">
        textFormat: Text.StyledText
        font.pointSize: 8
        font.capitalization: Font.AllUppercase
    }

    Row {
        id: rightButtons
        anchors.right: parent.right
        anchors.rightMargin: header.margin
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -separator.height / 2

        IconButton {
            visible: header.showSearch
            icon: "search"
            pressedIcon: "search_inv"
            fallbackText: "Search"
            onClicked: header.searchClicked()
        }
        IconButton {
            id: menuButton
            // bw_menu_close_b is the inverted look: shown while pressed and while the menu is open
            icon: header.menuOpen ? "bw_menu_close_b" : "bw_menu_open_b"
            pressedIcon: "bw_menu_close_b"
            fallbackText: "Menu"
            onClicked: header.menuClicked()
        }
    }

    Rectangle {
        id: separator
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: "black"
    }
}
