import QtQuick
import QtTest

// main.qml's selector wiring, against stand-ins for the C++ objects. Any QML warning fails a
// test.
Item {
    TestCase {
        name: "MainWindow"
        when: windowShown

        function init() {
            warningLog.watch()
            warningLog.take()
            mockServices.online = true
            mockServices.genresStatus = "idle"
            mockServices.genresTotal = -1
            mockServices.genresProgress = 0
            mockServices.genresCached = false
            mockServices.genreLoads = 0
            mockServices.genresList.clear()
        }
        function cleanup() {
            compare(warningLog.take(), "")
        }
        function create() {
            const component = Qt.createComponent("../../src/qml/main.qml")
            verify(component.status === Component.Ready, component.errorString())
            const window = component.createObject(null, {visible: false})
            verify(window !== null)
            return window
        }
        function test_fallbackDialogBlocksThePageKeys() {
            const window = create()
            compare(window.overlayOpen, false)
            mockServices.libraryFallback()
            const dialog = findChild(window, "libraryFallbackDialog")
            verify(dialog !== null)
            compare(dialog.visible, true)
            compare(window.overlayOpen, true)
            dialog.visible = false
            compare(window.overlayOpen, false)
            window.destroy()
        }
        function test_menusOpenAtTheirWidthWithoutBindingLoops() {
            const window = create()
            const headerMenu = findChild(window, "headerMenu")
            const libraryMenu = findChild(window, "libraryMenu")
            headerMenu.visible = true
            verify(headerMenu.width <= window.width)
            headerMenu.visible = false
            libraryMenu.visible = true
            verify(libraryMenu.width <= window.width)
            window.destroy()
        }
        // Genres T10, T27: the fifth tab, and the shared menus and keys follow it
        function test_genresTabTargetsItsOwnList() {
            const window = create()
            const footer = findChild(window, "footer")
            const page = findChild(window, "genresPage")
            verify(footer !== null && page !== null)
            compare(footer.tabs.length, 5)
            compare(footer.tabs[4].label, "Genres")
            compare(footer.tabs[3].label, "Authors")
            compare(page.visible, false)
            footer.tabClicked(4)
            compare(window.showingGenres, true)
            compare(page.visible, true)
            compare(mockServices.genreLoads, 1)
            verify(window.activeList === page.list)
            verify(window.frontPageButtons === page.pageButtons)
            compare(window.activeQuery.sortOptions, ["Name", "Book count"])
            compare(window.activeQuery.hasFilter, false)
            window.activeQuery.sortIndex = 1
            window.activeQuery.applied()
            compare(mockServices.genreSort, ["bookCount", true])
            page.groupClicked({name: " Sci-Fi"})
            compare(window.booksOfField, "genre")
            compare(mockServices.booksOfShown[0], "genre")
            compare(mockServices.booksOfShown[1], [" Sci-Fi"])
            compare(window.showingBooksOf, true)
            window.destroy()
        }
        // T19: what the list says about itself, per status
        function test_genresNoticeFollowsTheStatus() {
            const window = create()
            const page = findChild(window, "genresPage")
            const cases = [
                ["idle", false, -1, ""],
                ["loading", false, -1, "Loading genres"],
                ["loading", false, 500, "Loading genres: 200 of 500 books"],
                ["loading", true, 500, "Updating genres"],
                ["incomplete", false, 500, "Some genres are missing. Tap Genres to try again."],
                ["stale", true, 500, "Couldn't update genres. Showing the last complete list. Tap Genres to try again."],
                ["failed", false, -1, ""],
                ["complete", false, 500, ""],
                ["local", false, -1, ""],
            ]
            mockServices.genresProgress = 200
            for (const c of cases) {
                mockServices.genresCached = c[1]
                mockServices.genresTotal = c[2]
                mockServices.genresStatus = c[0]
                compare(page.notice, c[3], c[0])
            }
            window.destroy()
        }
        // T11, T16, T19: when the visible genres load again
        function test_genresReloadOnlyWhenNotCompleteOrLoading() {
            const window = create()
            const footer = findChild(window, "footer")
            footer.tabClicked(4)
            compare(mockServices.genreLoads, 1)
            // Leaving and coming back
            for (const status of ["complete", "loading", "incomplete", "stale", "failed"]) {
                mockServices.genresStatus = status
                const before = mockServices.genreLoads
                footer.tabClicked(0)
                footer.tabClicked(4)
                compare(mockServices.genreLoads - before, status === "complete" || status === "loading" ? 0 : 1, status)
            }
            // Tapping the current tab
            for (const status of ["complete", "loading", "incomplete", "stale", "failed"]) {
                mockServices.genresStatus = status
                const before = mockServices.genreLoads
                footer.tabClicked(4)
                compare(mockServices.genreLoads - before, status === "complete" || status === "loading" ? 0 : 1, status)
            }
            // Back online
            for (const status of ["complete", "incomplete", "local"]) {
                mockServices.genresStatus = status
                mockServices.online = false
                const before = mockServices.genreLoads
                mockServices.online = true
                compare(mockServices.genreLoads - before, status === "complete" ? 0 : 1, status)
            }
            // A library switch with the tab visible
            mockServices.genresStatus = "complete"
            const before = mockServices.genreLoads
            mockServices.librarySwitched()
            compare(mockServices.genreLoads - before, 1)
            window.destroy()
        }
        function test_libraryMenuBlocksThePageKeys() {
            const window = create()
            const menu = findChild(window, "libraryMenu")
            verify(menu !== null)
            menu.visible = true
            compare(window.overlayOpen, true)
            mockServices.librarySwitched()
            compare(menu.visible, false)
            compare(window.overlayOpen, false)
            window.destroy()
        }
    }
}
