import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    objectName: "safetyDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 60, 740)
    height: Math.min(host.height - 60, 620)
    modal: true
    title: qsTr("Safety & history")
    standardButtons: Dialog.Close
    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroller.availableWidth
            spacing: 12
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Undo restores configuration files, not every runtime property. Reopen affected windows if needed. Snapshots contain configuration content; keep them private.") }
            Flow {
                Layout.fillWidth: true; spacing: 8
                Button { text: qsTr("Undo"); enabled: backend.canUndo && !dialog.host.dirty; onClicked: backend.undo() }
                Button { text: qsTr("Redo"); enabled: backend.canRedo && !dialog.host.dirty; onClicked: backend.redo() }
                Button { text: qsTr("Reload external edits"); onClicked: backend.reloadFromDisk() }
                Button { text: qsTr("Discard draft…"); enabled: dialog.host.dirty; onClicked: dialog.host.discardCurrentDraft() }
                Button { text: backend.trialActive ? qsTr("Revert trial") : qsTr("Try geometry · 15s"); onClicked: backend.trialActive ? backend.revertTrial() : backend.tryGeometry(dialog.host.options(), 15) }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Try changes only size, position and floating/tiled mode. No appearance changes or rule writes. Automatic revert requires this process to remain alive.") }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: backupName; Layout.fillWidth: true; placeholderText: qsTr("Snapshot name"); Accessible.name: qsTr("Snapshot name") }
                Button { text: qsTr("Create snapshot"); onClicked: backend.createBackup(backupName.text) }
            }
            Repeater {
                model: backend.backups
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: modelData.name + " · " + modelData.date; elide: Text.ElideRight }
                    Button { text: qsTr("Restore…"); onClicked: { restoreConfirm.backupId = modelData.id; restoreConfirm.open() } }
                }
            }
            Label { text: qsTr("Recent configuration changes"); font.bold: true }
            Repeater {
                model: backend.history
                delegate: Label { required property var modelData; Layout.fillWidth: true; text: modelData.date + " · " + modelData.name; elide: Text.ElideRight }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: backend.status }
        }
    }
    StudioDialog {
        id: restoreConfirm
        property string backupId
        anchors.centerIn: parent
        modal: true
        title: qsTr("Restore all three configuration files?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { width: 340; wrapMode: Text.WordWrap; text: qsTr("This replaces the rule collection, generated Lua module and main Lua configuration with this snapshot. The current state is kept for undo.") }
        onAccepted: backend.restoreBackup(backupId)
    }
}
