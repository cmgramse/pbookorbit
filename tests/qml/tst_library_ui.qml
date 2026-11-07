import QtQuick
import QtTest
import "../../src/qml"
import "../../src/qml/libraryui.js" as LibraryUi

Item {
    id: root
    width: 480
    height: 640

    QtObject {
        id: mockClient
        property var libraries: []
        property int currentLibraryId: 1
        property string currentLibraryLabel: "Alpha"
        property var selected: []
        signal librarySwitched()
        signal libraryFallback()
        function selectLibrary(id) { selected.push(id) }
    }

    Component {
        id: menuComponent
        LibraryMenu {
            client: mockClient
            maxWidth: 300
        }
    }
    Component {
        id: handlerComponent
        LibrarySwitchHandler { client: mockClient }
    }
    Component {
        id: spyComponent
        SignalSpy {}
    }

    TestCase {
        name: "LibraryUi"
        when: windowShown

        function test_selectorNeedsTwoLibrariesOnlineAndSignedIn() {
            compare(LibraryUi.selectorVisible(true, true, 0), false)
            compare(LibraryUi.selectorVisible(true, true, 1), false)
            compare(LibraryUi.selectorVisible(true, true, 2), true)
            compare(LibraryUi.selectorVisible(true, false, 2), false)
            compare(LibraryUi.selectorVisible(false, true, 2), false)
        }
        function test_downloadedLabel() {
            compare(LibraryUi.downloadedLabel(1, false), "Downloaded only")
            compare(LibraryUi.downloadedLabel(2, false), "Downloaded, all libraries")
            compare(LibraryUi.downloadedLabel(2, true), "Downloaded only")
        }
        function test_emptyText() {
            compare(LibraryUi.emptyTitle(0), "No books")
            compare(LibraryUi.emptyTitle(1), "No ebooks")
            compare(LibraryUi.emptyTitle(2), "No audiobooks")
            compare(LibraryUi.emptyText(0), "This library has no books.")
            compare(LibraryUi.emptyText(1), "Choose All titles to see every book.")
        }
        function test_everyQmlFileCompiles() {
            verify(qmlFileCount > 30)
            compare(qmlCompileErrors.join("\n"), "")
        }

        function spy(target, signalName) {
            return createTemporaryObject(spyComponent, root, {target: target, signalName: signalName})
        }
        function test_switchClosesAndScrolls() {
            const handler = createTemporaryObject(handlerComponent, root)
            const closed = spy(handler, "closeAll")
            const scrolled = spy(handler, "scrollToTop")
            mockClient.librarySwitched()
            compare(closed.count, 1)
            compare(scrolled.count, 1)
        }
        function test_switchReloadsOnlyTheVisibleTab() {
            const handler = createTemporaryObject(handlerComponent, root, {showingSeries: true})
            const series = spy(handler, "reloadSeries")
            const authors = spy(handler, "reloadAuthors")
            mockClient.librarySwitched()
            compare(series.count, 1)
            compare(authors.count, 0)
            handler.showingSeries = false
            handler.showingAuthors = true
            mockClient.librarySwitched()
            compare(series.count, 1)
            compare(authors.count, 1)
        }
        function test_noReloadWithoutALibrary() {
            const handler = createTemporaryObject(handlerComponent, root, {showingSeries: true, showingAuthors: true})
            const series = spy(handler, "reloadSeries")
            const authors = spy(handler, "reloadAuthors")
            mockClient.currentLibraryId = -1
            mockClient.librarySwitched()
            mockClient.currentLibraryId = 1
            compare(series.count, 0)
            compare(authors.count, 0)
        }
        function test_fallbackCarriesTheLabel() {
            const handler = createTemporaryObject(handlerComponent, root)
            const fallback = spy(handler, "fallback")
            mockClient.libraryFallback()
            compare(fallback.count, 1)
            compare(fallback.signalArguments[0][0], "Alpha")
        }

        function rows(count, label) {
            const list = []
            for (let i = 1; i <= count; i++)
                list.push({id: i, name: label(i), label: label(i), bookCount: 1})
            return list
        }
        function openMenu(libraries, properties) {
            mockClient.libraries = libraries
            const menu = createTemporaryObject(menuComponent, root, properties || {})
            menu.visible = true
            const list = findChild(menu, "libraryList")
            verify(list !== null)
            return {menu: menu, list: list}
        }
        function test_longLabelsStayWithinMaxWidth() {
            const wide = "A very long library name, longer than any screen ".repeat(4)
            const opened = openMenu(rows(2, function(i) { return wide + i }))
            compare(opened.menu.width, 300)
            tryVerify(function() { return opened.list.itemAtIndex(0) !== null })
            const row = opened.list.itemAtIndex(0)
            verify(row.x + row.width <= opened.menu.width)
        }
        function test_shortLabelsKeepTheMenusMinimumWidth() {
            const opened = openMenu(rows(2, function(i) { return "L" + i }))
            verify(opened.menu.width >= 222 && opened.menu.width < 300)
        }
        function test_manyLibrariesScrollWithinTheHeightCap() {
            const opened = openMenu(rows(24, function(i) { return "Library " + i }))
            compare(opened.list.height, 7 * 50)
            verify(opened.list.interactive)
            verify(opened.list.contentHeight > opened.list.height)
        }
        function test_fewLibrariesDontScroll() {
            const opened = openMenu(rows(2, function(i) { return "Library " + i }))
            compare(opened.list.height, 2 * 50)
            verify(!opened.list.interactive)
        }
        function test_heightStaysWithinTheWindow() {
            const opened = openMenu(rows(24, function(i) { return "Library " + i }), {availableHeight: 300})
            verify(opened.menu.height <= 300)
        }
        function test_marksTheCurrentLibraryAndClickSelectsOnce() {
            mockClient.selected = []
            const opened = openMenu(rows(3, function(i) { return "Library " + i }))
            tryVerify(function() { return opened.list.itemAtIndex(1) !== null })
            compare(opened.list.itemAtIndex(0).checked, true)
            compare(opened.list.itemAtIndex(1).checked, false)
            mouseClick(opened.list.itemAtIndex(1))
            compare(opened.menu.visible, false)
            compare(mockClient.selected.length, 1)
            compare(mockClient.selected[0], 2)
        }
        function test_duplicateLabelsAreKept() {
            const opened = openMenu([{id: 1, label: "Same #1"}, {id: 2, label: "Same #2"}])
            tryVerify(function() { return opened.list.itemAtIndex(1) !== null })
            compare(opened.list.itemAtIndex(0).text, "Same #1")
            compare(opened.list.itemAtIndex(1).text, "Same #2")
        }
    }
}
