import QtQuick
import "units.js" as Units

// The Collections view: the user's book collections (bookOrbit.collections), one
// CollectionRow each, under a sub-toolbar with just the sort button, with a jump rail
// when sorted by name. Goes between the header and the footer; main.qml owns the sort
// menu, as it does the library's.
Item {
    id: page
    property string sortText
    property bool sortReversed: false
    property bool sortOpen: false
    property int tabOverhang: Units.dp(4)
    // Z to A when the sort is turned round
    property bool descending: false
    // Where the jump rail's first entry goes, in the page's coordinates, and its spacing
    property real jumpRailTop: NaN
    property real jumpRailPitch: NaN
    readonly property alias list: collectionList

    signal sortClicked()
    signal sortIconClicked()
    signal collectionClicked(var collection)

    ListView {
        id: collectionList
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: bookOrbit.collections

        // The rows are all there at once: nothing to load. The jump rail calls this.
        function loadVisibleBooks() {}

        // The sort bar and any status line scroll away with the rows, as in the library
        header: Column {
            width: ListView.view.width
            bottomPadding: status.visible ? Units.dp(8) : 0

            SubToolbar {
                width: parent.width
                showFilter: false
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
                visible: bookOrbit.error !== "" || collectionList.count === 0
                topPadding: Units.dp(8)
                text: bookOrbit.error !== "" ? bookOrbit.error
                      : !bookOrbit.collectionsLoaded ? "Working…"
                      : "No collections"
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

        delegate: CollectionRow {
            required property var book
            collection: book
            onClicked: page.collectionClicked(book)
        }
    }

    ListScrollbar {
        list: collectionList
        bottomInset: pageButtons.height
    }
    JumpRail {
        list: collectionList
        sections: bookOrbit.collectionJumpSections
        descending: page.descending
        topInset: collectionList.headerItem ? collectionList.headerItem.height : 0
        bottomInset: pageButtons.height
        alignTop: page.jumpRailTop
        alignPitch: page.jumpRailPitch
    }
    PageButtons {
        id: pageButtons
        list: collectionList
        anchors.bottom: collectionList.bottom
    }
}
