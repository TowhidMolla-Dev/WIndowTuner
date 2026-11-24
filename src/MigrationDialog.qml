import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StudioDialog {
    id: dialog
    objectName: "migrationDialog"
    required property var host
    property var review: ({})
    property string feedback: ""
    title: qsTr("Encrypted settings transfer")
    anchors.centerIn: parent
    width: Math.min(host.width - 48, 820)
    height: Math.min(host.height - 48, 720)
    modal: true
    standardButtons: Dialog.Close
    onClosed: {
        password.clear()
        bundle.clear()
        review = ({})
        feedback = ""
    }

    contentItem: ColumnLayout {
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Copy an encrypted bundle to another machine and unlock it there. Bundles include rules, profiles, scenes and workspace pins—not launch commands, session snapshots, preferences or physical display settings. Nothing is uploaded.")
        }
        TextField {
            id: password
            objectName: "migrationPassphrase"
            Layout.fillWidth: true
            placeholderText: qsTr("Bundle passphrase")
            Accessible.name: qsTr("Bundle passphrase")
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
            onTextChanged: { dialog.review = ({}); dialog.feedback = "" }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            clip: true
            TextArea {
                id: bundle
                objectName: "migrationBundle"
                placeholderText: qsTr("Paste an encrypted bundle here, or create one to copy.")
                Accessible.name: qsTr("Encrypted bundle text")
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                onTextChanged: { dialog.review = ({}); dialog.feedback = "" }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Create encrypted bundle")
                enabled: password.text.trim().length >= 12
                onClicked: {
                    const result = backend.exportMigration(password.text)
                    if (result.bundle) { bundle.text = result.bundle; bundle.selectAll() }
                    dialog.feedback = result.error || qsTr("Encrypted bundle ready to copy. Keep its passphrase separate.")
                }
            }
            Button {
                objectName: "previewMigrationAction"
                text: qsTr("Preview import")
                enabled: password.text.length > 0 && bundle.text.trim().length > 0
                onClicked: {
                    dialog.review = backend.previewMigration(bundle.text, password.text)
                    comparison.currentIndex = 1
                    dialog.feedback = dialog.review.error || (dialog.review.changed ? "" : qsTr("No changes to import."))
                }
            }
        }
        Label {
            objectName: "migrationFeedback"
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            text: dialog.feedback
        }
        Label {
            objectName: "migrationWarnings"
            Layout.fillWidth: true
            visible: !!dialog.review.token
            wrapMode: Text.Wrap
            text: {
                const apps = dialog.review.missingApps || []
                const displays = dialog.review.missingDisplays || []
                const lines = []
                if (apps.length) lines.push(qsTr("App classes not observed or hinted on this machine: %1").arg(apps.join(", ")))
                if (displays.length) lines.push(qsTr("Displays not currently connected: %1").arg(displays.join(", ")))
                return lines.length ? lines.join("\n") : qsTr("No unresolved literal app or display references found. Class hints and patterns are not proof of a match.")
            }
            maximumLineCount: 4
            elide: Text.ElideRight
            ToolTip.visible: warningHover.hovered
            ToolTip.text: text
            HoverHandler { id: warningHover }
        }
        TabBar {
            id: comparison
            Layout.fillWidth: true
            visible: !!dialog.review.token
            TabButton { text: qsTr("Before · this machine") }
            TabButton { text: qsTr("After · imported settings") }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            TextArea {
                objectName: "migrationComparison"
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                text: (comparison.currentIndex === 0 ? dialog.review.before : dialog.review.after) || ""
                Accessible.name: qsTr("Portable settings comparison")
            }
        }
        Button {
            objectName: "reviewMigrationAction"
            Layout.alignment: Qt.AlignRight
            text: qsTr("Review replacement…")
            enabled: !!dialog.review.changed && !dialog.host.dirty && !backend.trialActive && !backend.monitorRevertPending
            onClicked: confirmation.open()
        }
    }

    StudioDialog {
        id: confirmation
        objectName: "confirmMigrationDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(dialog.width - 12, 490)
        title: qsTr("Replace portable settings?")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        contentItem: Label {
            wrapMode: Text.WordWrap
            text: qsTr("This replaces the active saved rules, profiles, scenes and pins with the reviewed bundle and reloads Hyprland. It won’t launch apps, restore scenes, or change physical display settings.")
        }
        onAccepted: {
            if (dialog.host.dirty) return
            const applied = backend.applyMigration(bundle.text, password.text, dialog.review.token)
            if (applied) {
                password.clear()
                bundle.clear()
            } else dialog.review = ({})
            dialog.feedback = backend.status
        }
    }
}
