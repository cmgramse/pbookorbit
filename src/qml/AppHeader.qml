import QtQuick
import "units.js" as Units
import "night.js" as Night

// Top toolbar: home button, title, optional search and menu buttons, and a 1px line below.
// Offline, a small crossed-out Wi-Fi icon follows the title.
// As in the system apps, home goes to the dashboard. The title is tappable too; six quick
// taps emit secretTapped(). Follows AppHeader.qml from the SDK's qml_test: theme icons at
// natural size, dp(15) from the edges. Taller than dp(56) if the theme's icons are.
Rectangle {
    id: header
    property alias title: titleText.text
    property bool menuOpen: false
    property bool showSearch: false
    property bool offline: false
    property bool showMenu: true
    signal homeClicked()
    signal titleClicked()
    signal secretTapped()
    signal searchClicked()
    signal menuClicked()

    readonly property int margin: Units.dp(15)
    height: Math.max(Units.dp(56), homeButton.height, rightButtons.height) + separator.height
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
        // Centred, so reserve the wider side's button width on both sides
        width: parent.width - 2 * (header.margin + Math.max(homeButton.width, rightButtons.width))
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        // Lets part of the title be coloured with <font color="...">
        textFormat: Text.StyledText
        font.pointSize: 8
        font.capitalization: Font.AllUppercase

        // Full toolbar height, across the title's width
        MouseArea {
            width: parent.width
            height: header.height - separator.height
            anchors.centerIn: parent
            // Counts presses, since Qt may report the second of two quick taps as a double
            // click rather than a click
            onPressed: {
                tapTimer.taps++
                tapTimer.restart()
                if (tapTimer.taps === 6) {
                    tapTimer.taps = 0
                    header.secretTapped()
                }
            }
            onClicked: header.titleClicked()
        }
        // Reset the count after a pause
        Timer {
            id: tapTimer
            property int taps: 0
            interval: 800
            onTriggered: taps = 0
        }
    }

    // Just after the title's text, outside its layout so the title stays centred: a red
    // badge with a crossed-out Wi-Fi sign (src/icons/offline.svg), a little taller than
    // the capitals. Keeps its colours in night mode.
    Image {
        id: offlineIcon
        visible: header.offline
        width: Units.dp(17)
        height: width
        sourceSize.width: width
        sourceSize.height: height
        x: titleText.x + (titleText.width + Math.min(titleText.contentWidth, titleText.width)) / 2 + Units.dp(5)
        anchors.verticalCenter: titleText.verticalCenter
        source: Night.image("qrc:/icons/offline.bmp")
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
            visible: header.showMenu
            // bw_menu_close_b is the inverted look, shown while pressed or while the menu
            // is open
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
