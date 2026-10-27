import QtQuick
import "units.js" as Units

// One field of an info page (BookInfoPage, AuthorInfoPage): a bold label over its value
// and a grey line under them; nothing when there is no value. A tappable field has a
// chevron at its right, is inverted while pressed, as InfoCells' cells are, and sends
// clicked(). A link field (a web address) underlines its value instead, and only the
// value's text is tapped, inverted while pressed; it sends linkClicked().
Item {
    id: field
    property string label
    property string value
    property bool richText: false
    property bool showLine: true
    property bool tappable: false
    property bool link: false
    signal clicked()
    signal linkClicked()
    visible: value !== ""
    implicitHeight: column.height

    // Pressed: black from the line above (the previous field's, halfway down its gap) to
    // its own
    Rectangle {
        visible: mouse.pressed
        y: -Units.dp(9) + 1
        width: parent.width
        height: column.height - 1
        color: "black"
    }

    // A link's text, black behind it while pressed
    Rectangle {
        visible: linkMouse.pressed
        x: valueText.x
        y: valueText.y
        width: valueText.contentWidth
        height: valueText.contentHeight
        color: "black"
    }

    Column {
        id: column
        width: parent.width
        spacing: Units.dp(6)

        Text {
            // Clear of the chevron
            width: parent.width - (field.tappable ? chevron.width + Units.dp(6) : 0)
            text: field.label
            color: mouse.pressed ? "white" : "black"
            font.pointSize: 8
            font.bold: true
            wrapMode: Text.Wrap
        }
        Text {
            id: valueText
            width: parent.width - (field.tappable ? chevron.width + Units.dp(6) : 0)
            text: field.value
            color: mouse.pressed || linkMouse.pressed ? "white" : "black"
            font.underline: field.link
            // Descriptions come as HTML
            textFormat: field.richText ? Text.RichText : Text.PlainText
            font.pointSize: 8
            wrapMode: Text.Wrap
        }
        // 21px under the value, then 61px to the next label's baseline
        Item {
            width: parent.width
            height: Units.dp(18)
            Rectangle {
                visible: field.showLine
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 1
                color: "#7f7f7f"
            }
        }
    }

    // As in InfoCells: the toolbar's back chevron turned to point right, level with the
    // label and value
    Image {
        id: chevron
        visible: field.tappable
        anchors.right: parent.right
        y: (valueText.y + valueText.height - height) / 2
        source: !field.tappable ? "" : mouse.pressed ? "image://theme/back@inverted" : "image://theme/back"
        mirror: true
    }
    MouseArea {
        id: linkMouse
        enabled: field.link
        x: valueText.x
        y: valueText.y
        width: valueText.contentWidth
        height: valueText.contentHeight
        onClicked: field.linkClicked()
    }
    MouseArea {
        id: mouse
        enabled: field.tappable
        y: -Units.dp(9)
        width: parent.width
        height: column.height
        onClicked: field.clicked()
    }
}
