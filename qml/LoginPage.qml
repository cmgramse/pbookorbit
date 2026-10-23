import QtQuick
import QtQuick.Controls
import "units.js" as Units

// Sign-in form, centred on the page: logo, labelled fields, status line, button.
// Uses the same username and password as the BookOrbit website.
Item {
    id: page
    // The form can move up under the header while the keyboard is open
    clip: true

    // True while either field has focus, for the quicker e-ink update mode
    readonly property bool typing: usernameField.activeFocus || passwordField.activeFocus

    function signIn() {
        if (bookOrbit.busy || usernameField.text === "" || passwordField.text === "")
            return
        // The button doesn't take focus, so the field would keep it and the system keyboard
        // would stay up: move focus off the fields and close the keyboard first
        page.forceActiveFocus()
        Qt.inputMethod.hide()
        bookOrbit.signIn(usernameField.text, passwordField.text)
        passwordField.text = ""
    }

    // How far the form is moved up so the focused field sits above the on-screen keyboard.
    // Set in one step (no animation: on e-ink that is only extra redraws).
    property real keyboardShift: 0

    function updateKeyboardShift() {
        const field = usernameField.activeFocus ? usernameField
                    : passwordField.activeFocus ? passwordField : null
        const keyboard = Qt.inputMethod.keyboardRectangle   // window coordinates
        if (!field || !Qt.inputMethod.visible || keyboard.height <= 0) {
            keyboardShift = 0
            return
        }
        // Where the field's bottom would be with the form unshifted, plus a gap
        const fieldBottom = field.mapToItem(null, 0, field.height).y + keyboardShift + Units.dp(16)
        keyboardShift = Math.max(0, fieldBottom - keyboard.y)
        diagnostics.log("keyboard at y " + keyboard.y + ", form moved up " + keyboardShift)
    }

    // The keyboard rectangle arrives a moment after the keyboard is reported visible
    Connections {
        target: Qt.inputMethod
        function onVisibleChanged() { Qt.callLater(page.updateKeyboardShift) }
        function onKeyboardRectangleChanged() { Qt.callLater(page.updateKeyboardShift) }
    }

    // Label above a text field, left aligned
    component FieldLabel: Text {
        width: parent.width
        font.pointSize: 8
        bottomPadding: Units.dp(2)
        font.capitalization: Font.AllUppercase
    }

    component FormField: TextField {
        width: parent.width
        font.pointSize: 9
        leftPadding: Units.dp(10)
        rightPadding: Units.dp(10)
        topPadding: Units.dp(8)
        bottomPadding: Units.dp(8)
        background: Rectangle {
            radius: Units.dp(4)
            border.width: Units.dp(1)
            border.color: "black"
        }
    }

    Column {
        id: form
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -page.keyboardShift
        width: Math.min(parent.width - 2 * Units.dp(20), Units.dp(360))
        spacing: Units.dp(6)

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/images/bookorbit-logo.png"
            width: Units.dp(96)
            height: width
            sourceSize: Qt.size(width, height)
            smooth: true
        }
        Text {
            width: parent.width
            topPadding: Units.dp(6)
            bottomPadding: Units.dp(18)
            text: bookOrbit.serverUrl
            font.pointSize: 7
            color: "#444"
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
        }

        FieldLabel { text: "Username" }
        FormField {
            id: usernameField
            placeholderText: "Your BookOrbit username"
            text: bookOrbit.username
            onAccepted: passwordField.forceActiveFocus()
            onActiveFocusChanged: Qt.callLater(page.updateKeyboardShift)
        }
        Item { width: 1; height: Units.dp(10) }

        FieldLabel { text: "Password" }
        FormField {
            id: passwordField
            placeholderText: "Your password"
            echoMode: TextInput.Password
            onAccepted: page.signIn()
            onActiveFocusChanged: Qt.callLater(page.updateKeyboardShift)
        }

        // Progress or the last error; keeps its space so the button doesn't jump
        Text {
            width: parent.width
            height: Math.max(implicitHeight, Units.dp(40))
            topPadding: Units.dp(10)
            text: bookOrbit.busy ? "Signing in…" : bookOrbit.error
            font.pointSize: 8
            wrapMode: Text.Wrap
        }

        PillButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Sign in"
            enabled: !bookOrbit.busy && usernameField.text !== "" && passwordField.text !== ""
            onClicked: page.signIn()
        }
    }
}
