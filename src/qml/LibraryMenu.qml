import QtQuick
import "units.js" as Units

// Drop-down list of the account's libraries. It scrolls past seven rows, as an account can
// have many, and long names are elided.
PopupMenu {
    id: libraryMenu
    property var client
    // The space below the menu's top, so it stays on screen
    property real availableHeight: Infinity

    visible: false
    sidePadding: 0

    MenuHeading {
        id: heading
        text: "Library"
    }
    Item {
        // As a RadioMenuItem measures itself, for the longest label
        implicitWidth: {
            let widest = 0
            for (const library of libraryMenu.client ? libraryMenu.client.libraries : [])
                widest = Math.max(widest, metrics.advanceWidth(library.label))
            return Units.dp(66) + Math.ceil(widest) + Units.dp(15)
        }
        width: parent ? parent.width : 0
        height: list.height

        FontMetrics {
            id: metrics
            font.pointSize: 8
        }
        ListView {
            id: list
            objectName: "libraryList"
            width: parent.width
            height: Math.min(contentHeight, 7 * Units.dp(50),
                             libraryMenu.availableHeight - heading.height
                             - 2 * (libraryMenu.border.width + libraryMenu.padding))
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            model: libraryMenu.client ? libraryMenu.client.libraries : []
            delegate: RadioMenuItem {
                required property var modelData
                required property int index
                width: list.width
                text: modelData.label
                checked: modelData.id === libraryMenu.client.currentLibraryId
                showSeparator: index < list.count - 1
                onClicked: {
                    libraryMenu.visible = false
                    libraryMenu.client.selectLibrary(modelData.id)
                }
            }
        }
        ListScrollbar {
            list: list
        }
    }
}
