import QtQuick
import "units.js" as Units

// Drop-down menu in the style of the system apps, measured from the library app on an
// Era Color: thick black border, rounded corners, rows inset from the border. The system
// squares off a corner only where a menu joins a tab control (the library's "ALL BOOKS"
// tab, Libby's filter tab): set joinedCorner for that. A menu opened from a button keeps
// all four corners rounded.
// Put PopupMenuItems inside (the last visible one should set showSeparator: false), or
// MenuHeadings and RadioMenuItems with sidePadding: 0 for a Libby-style options menu.
Rectangle {
    id: menu
    default property alias items: column.data
    // "", "topLeft" or "topRight": the corner that joins a tab
    property string joinedCorner: ""
    property int sidePadding: padding

    // At least 484px (the Sudoku menu, whose rows are short), otherwise as wide as the
    // widest visible row (the library menu)
    width: Math.max(Units.dp(222), contentWidth + 2 * (border.width + sidePadding))
    readonly property real contentWidth: {
        let widest = 0
        for (const row of column.children) {
            if (row.visible)
                widest = Math.max(widest, row.implicitWidth)
        }
        return widest
    }
    height: column.height + 2 * (border.width + padding)
    readonly property int padding: Units.dp(8)  // 16px
    radius: Units.dp(9)  // 20px
    topLeftRadius: joinedCorner === "topLeft" ? 0 : radius
    topRightRadius: joinedCorner === "topRight" ? 0 : radius
    border.width: Units.dp(4)  // 9px
    border.color: "black"
    color: "white"

    Column {
        id: column
        x: menu.border.width + menu.sidePadding
        y: menu.border.width + menu.padding
        width: menu.width - 2 * x
    }
}
