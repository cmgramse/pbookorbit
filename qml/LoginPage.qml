import QtQuick
import QtQuick.Controls
import "units.js" as Units
import "night.js" as Night

// Sign-in form, centred on the page: server, username and password fields, status line,
// button. Uses the same username and password as the BookOrbit website. The server is a
// field until a sign-in has worked, then a pill whose cross forgets it.
Item {
    id: page
    // The form can move up under the header while the keyboard is open
    clip: true

    // True while either field has focus, for the quicker e-ink update mode
    readonly property bool typing: serverField.activeFocus || usernameField.activeFocus
                                   || passwordField.activeFocus
    readonly property string server: bookOrbit.serverUrl !== "" ? bookOrbit.serverUrl : serverField.text.trim()
    readonly property bool canSignIn: !bookOrbit.busy && server !== "" && usernameField.text !== ""
                                      && passwordField.text !== ""

    function signIn() {
        if (!canSignIn)
            return
        // The button doesn't take focus, so the field would keep it and the system keyboard
        // would stay up: move focus off the fields and close the keyboard first
        page.forceActiveFocus()
        Qt.inputMethod.hide()
        bookOrbit.signIn(page.server, usernameField.text, passwordField.text)
        passwordField.text = ""
    }

    // How far the form is moved up so the focused field sits above the on-screen keyboard.
    // Set in one step (no animation: on e-ink that is only extra redraws).
    property real keyboardShift: 0

    function updateKeyboardShift() {
        const field = serverField.activeFocus ? serverField
                    : usernameField.activeFocus ? usernameField
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

    // A field's name, tucked under its underline, left aligned
    component FieldLabel: Text {
        width: parent.width
        topPadding: Units.dp(3)
        font.pointSize: 8
        font.capitalization: Font.AllUppercase
    }

    // On a faint grey fill, with just an underline, thicker while typing in it
    component FormField: TextField {
        id: field
        width: parent.width
        font.pointSize: 8
        leftPadding: Units.dp(10)
        rightPadding: Units.dp(10)
        topPadding: Units.dp(10)
        bottomPadding: Units.dp(10)
        background: Rectangle {
            color: "#f5f5f5"
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                // Device pixels, like the native buttons' borders
                height: field.activeFocus ? 4 : 2
                color: "black"
            }
        }
    }

    Column {
        id: form
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -page.keyboardShift
        width: Math.min(parent.width - 2 * Units.dp(20), Units.dp(360))
        spacing: Units.dp(18)

        // As high with the pill as with the field and its label, so the form below stays put
        // when the server is forgotten. The label's space sets the pill apart from the form.
        Column {
            width: parent.width
            height: serverField.implicitHeight + serverLabel.implicitHeight
            FormField {
                id: serverField
                visible: bookOrbit.serverUrl === ""
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onAccepted: usernameField.forceActiveFocus()
                onActiveFocusChanged: Qt.callLater(page.updateKeyboardShift)
            }
            // The saved server, in the user's accent colour, as high as a field
            Rectangle {
                id: serverPill
                visible: bookOrbit.serverUrl !== ""
                width: parent.width
                height: serverField.implicitHeight
                radius: height / 2
                color: Night.color(bookOrbit.accentColor)
                readonly property color textColor: Night.color(bookOrbit.accentTextColor)

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Units.dp(16)
                    anchors.right: forget.left
                    anchors.verticalCenter: parent.verticalCenter
                    // https:// is assumed, so only http:// is worth showing
                    text: bookOrbit.serverUrl.replace(/^https:\/\//, "")
                    color: serverPill.textColor
                    font.pointSize: 8
                    elide: Text.ElideMiddle
                }
                IconButton {
                    id: forget
                    anchors.right: parent.right
                    anchors.rightMargin: Units.dp(4)
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.height
                    height: parent.height
                    // The theme's cross is black: turned white for the darker accents
                    icon: serverPill.textColor.hslLightness > 0.5 ? "close@inverted" : "close"
                    iconSize: Units.dp(20)
                    fallbackText: "×"
                    enabled: !bookOrbit.busy
                    onClicked: {
                        serverField.text = ""
                        bookOrbit.forgetServer()
                    }
                }
            }
            FieldLabel {
                id: serverLabel
                visible: serverField.visible
                text: "Server"
            }
        }

        Column {
            width: parent.width
            FormField {
                id: usernameField
                text: bookOrbit.username
                inputMethodHints: Qt.ImhNoAutoUppercase
                onAccepted: passwordField.forceActiveFocus()
                onActiveFocusChanged: Qt.callLater(page.updateKeyboardShift)
            }
            FieldLabel { text: "Username" }
        }

        Column {
            width: parent.width
            FormField {
                id: passwordField
                echoMode: TextInput.Password
                onAccepted: page.signIn()
                onActiveFocusChanged: Qt.callLater(page.updateKeyboardShift)
            }
            FieldLabel { text: "Password" }
        }

        // The last error; keeps its space so the button doesn't jump
        Text {
            width: parent.width
            height: Math.max(implicitHeight, Units.dp(40))
            text: bookOrbit.busy ? "" : bookOrbit.error
            font.pointSize: 8
            wrapMode: Text.Wrap
        }

        PillButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Sign in"
            busy: bookOrbit.busy
            enabled: page.canSignIn
            onClicked: page.signIn()
        }
    }
}
