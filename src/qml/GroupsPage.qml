import QtQuick
import "units.js" as Units

// The Collections or Series view: one GroupRow per group (bookOrbit.collections or
// seriesList) under a sort-only sub-toolbar, with a jump rail when sorted by name. Sits
// between the header and footer; main.qml owns the sort menu, as for the library.
Item {
    id: page
    // For the page-turn keys, which main.qml forwards to the front page
    readonly property alias pageButtons: pageButtons
    property string sortText
    property bool sortReversed: false
    property bool sortOpen: false
    // Authors also have the library's filter
    property bool hasFilter: false
    property string filterText
    property bool filterOpen: false
    property int tabOverhang: Units.dp(4)
    // Z to A when the sort is reversed
    property bool descending: false
    // Y of the jump rail's first entry (page coordinates) and its spacing
    property real jumpRailTop: NaN
    property real jumpRailPitch: NaN
    // The model and jump sections, whether they've loaded, and the text to show when there
    // are none
    property var model
    property var jumpSections: []
    property bool loaded: false
    property string emptyText
    readonly property alias list: groupList

    signal filterClicked()
    signal sortClicked()
    signal sortIconClicked()
    signal groupClicked(var group)

    ListView {
        id: groupList
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: page.model

        // All rows are present at once, so there's nothing to load. Called by the jump
        // rail.
        function loadVisibleBooks() {}

        // The sort bar and any status line scroll with the rows, as in the library
        header: Column {
            width: ListView.view.width
            bottomPadding: status.visible ? Units.dp(8) : 0

            SubToolbar {
                width: parent.width
                showFilter: page.hasFilter
                filterText: page.filterText
                filterOpen: page.filterOpen
                onFilterClicked: page.filterClicked()
                sortText: page.sortText
                sortReversed: page.sortReversed
                sortOpen: page.sortOpen
                tabOverhang: page.tabOverhang
                onSortClicked: page.sortClicked()
                onSortIconClicked: page.sortIconClicked()
            }
            Text {
                id: status
                x: Units.dp(26)
                width: parent.width - 2 * x
                visible: bookOrbit.error !== "" || (groupList.count === 0 && page.loaded)
                topPadding: Units.dp(8)
                text: bookOrbit.error !== "" ? bookOrbit.error : page.emptyText
                font.pointSize: 8
                wrapMode: Text.Wrap
            }
        }

        // Fast black-and-white updates while scrolling, as in the library list
        onMovementStarted: {
            const pos = mapToGlobal(0, 0)
            eink.enterA2ForFlick(pos.x, pos.y, width, height)
        }
        onMovementEnded: eink.leaveA2Soon()
        onVisibleChanged: if (!visible) eink.leaveA2()

        delegate: GroupRow {
            required property var book
            group: book
            onClicked: page.groupClicked(book)
        }
    }

    ListScrollbar {
        list: groupList
        bottomInset: pageButtons.height
    }
    Spinner {
        list: groupList
        visible: bookOrbit.error === "" && groupList.count === 0 && !page.loaded
    }
    JumpRail {
        list: groupList
        sections: page.jumpSections
        descending: page.descending
        topInset: groupList.headerItem ? groupList.headerItem.height : 0
        bottomInset: pageButtons.height
        alignTop: page.jumpRailTop
        alignPitch: page.jumpRailPitch
    }
    PageButtons {
        id: pageButtons
        list: groupList
        anchors.bottom: groupList.bottom
    }
}
