import QtQuick
import QtTest

// main.qml's selector wiring, against stand-ins for the C++ objects. Only the selector is
// looked at, and the stand-ins leave the rest of the app empty, so only binding loops count
// among its warnings.
Item {
    TestCase {
        name: "MainWindow"
        when: windowShown

        function init() {
            warningLog.watch()
            warningLog.take()
        }
        function cleanup() {
            verify(warningLog.take().indexOf("Binding loop") < 0)
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
