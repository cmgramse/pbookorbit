import QtQuick
import "units.js" as Units

// The Collections or Series view: groups of books (bookOrbit.collections or seriesList),
// one GroupRow each, under a sub-toolbar with just the sort button, with a jump rail when
// sorted by name. Goes between the header and the footer; main.qml owns the sort menu, as
// it does the library's.
Item {
    id: page
    property string sortText
    property bool sortReversed: false
    property bool sortOpen: false
    // The authors also have the library's filter
    property bool hasFilter: false
    property string filterText
    property bool filterOpen: false
    property int tabOverhang: Units.dp(4)
    // Z to A when the sort is turned round
    property bool descending: false
    // Where the jump rail's first entry goes, in the page's coordinates, and its spacing
    property real jumpRailTop: NaN
    property real jumpRailPitch: NaN
    // The groups' model and letters to jump to, whether they have been listed, and what to
    // say when there are none
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

        // The rows are all there at once: nothing to load. The jump rail calls this.
        function loadVisibleBooks() {}

        // The sort bar and any status line scroll away with the rows, as in the library
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
                visible: bookOrbit.error !== "" || groupList.count === 0
                topPadding: Units.dp(8)
                text: bookOrbit.error !== "" ? bookOrbit.error
                      : !page.loaded ? "Working…"
                      : page.emptyText
                font.pointSize: 8
                wrapMode: Text.Wrap
            }
        }

        // Fast black-and-white updates while it scrolls, as in the library list
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
