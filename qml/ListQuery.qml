import QtQuick

// One view's sort and filter, as its sub-toolbar and menus choose them: the library's,
// the collections', a collection's books'. applied() follows each change, once a new sort
// and its direction have both settled.
QtObject {
    // The sorts' names, their BookOrbit sort fields, and each one's usual direction
    property var sortOptions: []
    property var sortFields: []
    property var sortDescendingByDefault: []
    // Whether the view has the filter button (content type and downloaded only)
    property bool hasFilter: true

    property int sortIndex: 0
    property bool sortReversed: false
    property int filterIndex: 0
    // Only the books on the device, along with the content type
    property bool downloadedOnly: false

    readonly property bool sortDescending: sortDescendingByDefault[sortIndex] !== sortReversed

    signal applied()

    // Worked out here rather than read from sortDescending: when the sort and sortReversed
    // change together, that binding may not have caught up yet
    function descending() {
        return sortDescendingByDefault[sortIndex] !== sortReversed
    }
    // Another sort starts the usual way round
    function chooseSort(index) {
        if (index === sortIndex)
            return
        sortReversed = false
        sortIndex = index
        applied()
    }
    function reverseSort() {
        sortReversed = !sortReversed
        applied()
    }
    function chooseFilter(index) {
        if (index === filterIndex)
            return
        filterIndex = index
        applied()
    }
    function setDownloadedOnly(on) {
        downloadedOnly = on
        applied()
    }
}
