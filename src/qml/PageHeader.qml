import QtQuick
import "units.js" as Units

// Toolbar for pages opened over the book list, modelled on the system library's Book info
// page (screenshot 11): back chevron on the left, centred capitalised title (e.g. "BOOK
// INFO / 99 BOTTLES OF OOP"), and a 1px line below.
Rectangle {
    id: header
    property alias title: titleText.text
    signal back()

    height: Units.dp(56) + 1
    color: "white"

    IconButton {
        id: backButton
        // Same position as on the search page
        x: Units.dp(7)
        anchors.verticalCenter: parent.verticalCenter
        icon: "back"
        pressedIcon: "back_inv"
        fallbackText: "Back"
        onClicked: header.back()
    }

    Text {
        id: titleText
        anchors.centerIn: parent
        // Centred, so reserve the back button's width on both sides
        width: parent.width - 2 * (backButton.x + backButton.width)
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
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
