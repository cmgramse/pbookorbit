import QtQuick
import "units.js" as Units

// One field of an info page (BookInfoPage, AuthorInfoPage): a bold label over its value
// and a grey line under them; nothing when there is no value
Column {
    id: field
    property string label
    property string value
    property bool richText: false
    property bool showLine: true
    visible: value !== ""
    spacing: Units.dp(6)

    Text {
        width: parent.width
        text: field.label
        font.pointSize: 8
        font.bold: true
        wrapMode: Text.Wrap
    }
    Text {
        width: parent.width
        text: field.value
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
