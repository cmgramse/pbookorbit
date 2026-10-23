import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: root
    visible: true
    width: screenWidth
    height: windowHeight
    color: "white"

    function signIn() {
        if (bookOrbit.busy || usernameField.text === "" || passwordField.text === "")
            return
        // PbButton doesn't take focus, so the field would keep it and the system keyboard
        // would stay up: move focus off the fields and close the keyboard first
        root.contentItem.forceActiveFocus()
        Qt.inputMethod.hide()
        bookOrbit.signIn(usernameField.text, passwordField.text)
        passwordField.text = ""
    }

    // Quicker e-ink updates while typing. Applied with callLater because moving focus
    // from one field to the other briefly leaves neither focused.
    readonly property bool typing: usernameField.activeFocus || passwordField.activeFocus
    onTypingChanged: Qt.callLater(() => eink.setDynamicUpdates(root.typing))

    // Big, flat, black-on-white button that suits e-ink
    component PbButton: Rectangle {
        property alias text: label.text
        signal clicked()

        implicitWidth: 360; implicitHeight: 100
        radius: 12
        border.width: 3
        color: mouse.pressed ? "black" : "white"
        opacity: enabled ? 1 : 0.4

        Text {
            id: label
            anchors.centerIn: parent
            color: mouse.pressed ? "white" : "black"
            font.pointSize: 9
        }
        MouseArea { id: mouse; anchors.fill: parent; onClicked: parent.clicked() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 24

        Text {
            text: "BookOrbit"
            font.pointSize: 14
            font.bold: true
        }
        Text {
            text: bookOrbit.signedIn ? bookOrbit.username + " @ " + bookOrbit.serverUrl
                                     : bookOrbit.serverUrl
            font.pointSize: 8
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Text {
            visible: bookOrbit.error !== "" || bookOrbit.busy
            text: bookOrbit.busy ? "Working…" : bookOrbit.error
            font.pointSize: 8
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        // Signed out: the same username and password as the BookOrbit website
        ColumnLayout {
            visible: !bookOrbit.signedIn
            spacing: 24
            Layout.fillWidth: true

            TextField {
                id: usernameField
                placeholderText: "Username"
                text: bookOrbit.username
                font.pointSize: 9
                Layout.fillWidth: true
            }
            TextField {
                id: passwordField
                placeholderText: "Password"
                echoMode: TextInput.Password
                font.pointSize: 9
                Layout.fillWidth: true
                onAccepted: root.signIn()
            }
            PbButton {
                text: "Sign in"
                enabled: !bookOrbit.busy && usernameField.text !== "" && passwordField.text !== ""
                onClicked: root.signIn()
            }
        }

        // Signed in: the most recently added books
        Text {
            visible: bookOrbit.signedIn
            text: "Recently added (" + bookOrbit.books.length + " of " + bookOrbit.totalBooks + ")"
            font.pointSize: 10
            font.bold: true
        }
        ListView {
            id: bookList
            visible: bookOrbit.signedIn
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            // No bounce at the ends: on e-ink it is only extra redraws
            boundsBehavior: Flickable.StopAtBounds
            model: bookOrbit.books

            // Fast black-and-white updates for just the list while it scrolls
            onMovementStarted: {
                const pos = mapToItem(null, 0, 0)
                eink.enterA2ForFlick(pos.x, pos.y, width, height)
            }
            // Sent once the momentum has stopped, not when the finger lifts
            onMovementEnded: eink.leaveA2()
            onVisibleChanged: if (!visible) eink.leaveA2()
            delegate: Column {
                width: ListView.view.width
                bottomPadding: 16
                Text {
                    width: parent.width
                    text: modelData.title
                    font.pointSize: 9
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: modelData.authors.join(", ")
                          + (modelData.files.length ? "  ·  " + modelData.files.map(f => f.format).join(", ") : "")
                    font.pointSize: 7
                    elide: Text.ElideRight
                }
            }
        }
        Item { visible: !bookOrbit.signedIn; Layout.fillHeight: true }

        RowLayout {
            spacing: 24
            PbButton {
                visible: bookOrbit.signedIn
                text: "Refresh"
                enabled: !bookOrbit.busy
                onClicked: bookOrbit.refreshBooks()
            }
            PbButton {
                visible: bookOrbit.signedIn
                text: "Sign out"
                onClicked: bookOrbit.signOut()
            }
            PbButton { text: "Exit"; onClicked: Qt.quit() }
        }
    }
}
