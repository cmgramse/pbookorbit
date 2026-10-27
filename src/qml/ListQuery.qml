import QtQuick

// One view's sort and filter, as chosen in its sub-toolbar menus (library, collections, a
// collection's books). Emits applied() after each change, once the sort and its direction
// have both settled.
QtObject {
    // Sort names, their BookOrbit sort fields, and each one's default direction
    property var sortOptions: []
    property var sortFields: []
    property var sortDescendingByDefault: []
    // Whether the view has the filter button (content type and downloaded only)
    property bool hasFilter: true

    property int sortIndex: 0
    property bool sortReversed: false
    property int filterIndex: 0
    // Only books on the device, combined with the content type
    property bool downloadedOnly: false

    readonly property bool sortDescending: sortDescendingByDefault[sortIndex] !== sortReversed

    signal applied()

    // Computed here rather than read from sortDescending, which may lag when the sort and
    // sortReversed change together
    function descending() {
        return sortDescendingByDefault[sortIndex] !== sortReversed
    }
    // A different sort starts in its default direction
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
