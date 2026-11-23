import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    objectName: "scenesDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 48, 800)
    height: Math.min(host.height - 48, 720)
    modal: true
    title: qsTr("Saved desktop scenes")
    standardButtons: Dialog.Close

    property string selectedId: ""
    property var preview: ({})
    property string selectedSnapshotId: ""
    property var snapshotPreview: ({})
    property string feedback: ""

    function selectScene(id) {
        selectedId = id
        preview = backend.previewScene(id)
    }

    onOpened: {
        selectedId = ""
        preview = ({})
        selectedSnapshotId = ""
        snapshotPreview = ({})
        feedback = ""
        sceneName.text = ""
    }

    contentItem: ScrollView {
        id: scroller
        objectName: "scenesScroll"
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: contentHeight > availableHeight ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        ColumnLayout {
            width: scroller.availableWidth
            spacing: 12

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#aebbc3"
                text: qsTr("Capture the active monitor and workspace. Scenes store app classes and relative window positions—not titles or window addresses. Duplicate app classes are paired from left to right, then top to bottom.")
            }

            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: sceneName
                    objectName: "sceneNameField"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Scene name")
                    Accessible.name: qsTr("Name for the new scene")
                    maximumLength: 80
                    onAccepted: if (captureSceneAction.enabled) captureSceneAction.clicked()
                }
                Button {
                    id: captureSceneAction
                    objectName: "captureSceneAction"
                    text: qsTr("Capture active desktop")
                    enabled: sceneName.text.trim().length > 0 && !dialog.host.dirty
                    onClicked: {
                        if (backend.captureScene(sceneName.text)) {
                            sceneName.text = ""
                            dialog.feedback = backend.status
                        } else {
                            dialog.feedback = backend.status
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                visible: dialog.host.dirty
                color: "#ffbd72"
                wrapMode: Text.WordWrap
                text: qsTr("Save or discard the current rule draft before capturing a scene.")
            }
            Label { text: qsTr("Session restore"); font.bold: true }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#aebbc3"
                text: qsTr("Optional snapshots track window open/close, workspace, monitor, floating, and fullscreen events. Titles and window addresses are never stored. Restore is always manual; missing apps are not launched.")
            }
            CheckBox {
                objectName: "sessionSnapshotsEnabled"
                text: qsTr("Save event-driven session snapshots")
                checked: backend.sessionSnapshotsEnabled
                enabled: !dialog.host.dirty
                Accessible.name: qsTr("Enable event-driven session snapshots")
                onToggled: {
                    backend.setSessionSnapshotsEnabled(checked)
                    dialog.feedback = backend.status
                }
            }
            ListView {
                id: snapshotsList
                objectName: "sessionSnapshotList"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 150)
                Layout.minimumHeight: Math.min(contentHeight, 42)
                clip: true
                spacing: 3
                model: backend.sessionSnapshots
                delegate: Button {
                    required property var modelData
                    width: ListView.view.width
                    objectName: "sessionSnapshotRow"
                    text: qsTr("%1  ·  %2 window(s)").arg(modelData.createdAt).arg(modelData.windowCount)
                    checkable: true
                    checked: dialog.selectedSnapshotId === modelData.id
                    Accessible.name: qsTr("Preview session snapshot from %1").arg(modelData.createdAt)
                    onClicked: {
                        dialog.selectedSnapshotId = modelData.id
                        dialog.snapshotPreview = backend.previewSessionSnapshot(modelData.id)
                    }
                }
            }
            Label {
                objectName: "sessionPreviewSummary"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: dialog.snapshotPreview.error ? "#ffbd72" : "#c2cdd6"
                text: dialog.selectedSnapshotId
                    ? (dialog.snapshotPreview.error ||
                       qsTr("%1 open window(s) match; %2 are missing. Restore will not launch apps.")
                       .arg(dialog.snapshotPreview.matched || 0).arg(dialog.snapshotPreview.missing || 0))
                    : qsTr("Select a snapshot to preview a manual restore.")
            }
            Repeater {
                model: dialog.snapshotPreview.items || []
                delegate: Label {
                    required property var modelData
                    Layout.fillWidth: true
                    text: (modelData.matched ? "✓  " : "—  ") + modelData["class"]
                    color: modelData.matched ? "#8bd5a5" : "#ffbd72"
                }
            }
            Button {
                objectName: "restoreSessionSnapshotAction"
                Layout.alignment: Qt.AlignRight
                text: qsTr("Restore snapshot…")
                enabled: dialog.selectedSnapshotId !== "" &&
                    dialog.snapshotPreview.matched > 0 && !dialog.snapshotPreview.error &&
                    !dialog.host.dirty && !backend.trialActive && !backend.monitorRevertPending
                onClicked: restoreConfirmation.open()
            }

            Label { text: qsTr("Saved scenes"); font.bold: true }
            ListView {
                id: sceneList
                objectName: "savedSceneList"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 220)
                Layout.minimumHeight: Math.min(contentHeight, 48)
                clip: true
                spacing: 4
                model: backend.scenes
                delegate: Button {
                    required property var modelData
                    objectName: "savedSceneRow"
                    width: ListView.view.width
                    text: qsTr("%1  ·  %2 window(s)  ·  %3 / workspace %4")
                        .arg(modelData.name).arg(modelData.windowCount)
                        .arg(modelData.monitor).arg(modelData.workspace)
                    checkable: true
                    checked: dialog.selectedId === modelData.id
                    Accessible.name: qsTr("Preview scene %1").arg(modelData.name)
                    onClicked: dialog.selectScene(modelData.id)
                }
            }
            Label {
                Layout.fillWidth: true
                visible: backend.scenes.length === 0
                color: "#93a4b3"
                text: qsTr("No scenes saved yet.")
            }

            Label { text: qsTr("Preview"); font.bold: true }
            Label {
                objectName: "scenePreviewSummary"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: dialog.preview.error ? "#ffbd72" : "#c2cdd6"
                text: dialog.selectedId
                    ? (dialog.preview.error || qsTr("%1 open window(s) match; %2 are missing. Missing apps will not be launched.")
                        .arg(dialog.preview.matched || 0).arg(dialog.preview.missing || 0))
                    : qsTr("Select a scene to preview its matching open windows.")
            }
            Repeater {
                model: dialog.preview.items || []
                delegate: Label {
                    required property var modelData
                    Layout.fillWidth: true
                    text: (modelData.matched ? "✓  " : "—  ") + modelData["class"]
                    color: modelData.matched ? "#8bd5a5" : "#ffbd72"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: dialog.feedback
                    color: "#aebbc3"
                    wrapMode: Text.WordWrap
                }
                Button {
                    objectName: "removeSceneAction"
                    text: qsTr("Remove scene…")
                    enabled: dialog.selectedId !== ""
                    onClicked: removeConfirmation.open()
                }
                Button {
                    objectName: "openSceneAction"
                    text: qsTr("Apply to matching windows…")
                    enabled: dialog.selectedId !== "" && dialog.preview.matched > 0 &&
                        !dialog.preview.error && !dialog.host.dirty &&
                        !backend.trialActive && !backend.monitorRevertPending
                    onClicked: applyConfirmation.open()
                    background: Rectangle { radius: 7; color: enabled ? theme.accent : "#303941" }
                    contentItem: Label {
                        text: qsTr("Apply to matching windows…")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: parent.enabled ? "#111820" : "#87939a"
                    }
                }
            }
        }
    }

    Dialog {
        id: restoreConfirmation
        objectName: "restoreSessionConfirmation"
        title: qsTr("Restore this session snapshot?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(host.width - 80, 520)
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label {
            objectName: "restoreSessionConfirmationText"
            width: 460
            wrapMode: Text.WordWrap
            text: qsTr("This will reposition %1 matching open window(s) across their saved displays and workspaces. %2 missing window(s) will be skipped. No applications will be launched. This cannot be automatically undone.")
                .arg(dialog.snapshotPreview.matched || 0)
                .arg(dialog.snapshotPreview.missing || 0)
        }
        onAccepted: {
            backend.restoreSessionSnapshot(dialog.selectedSnapshotId)
            dialog.feedback = backend.status
            backend.refreshWindows()
            dialog.snapshotPreview = backend.previewSessionSnapshot(dialog.selectedSnapshotId)
        }
    }

    Dialog {
        id: applyConfirmation
        objectName: "applySceneConfirmation"
        title: qsTr("Apply this scene?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(host.width - 80, 520)
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label {
            objectName: "applySceneConfirmationText"
            width: 460
            wrapMode: Text.WordWrap
            text: qsTr("This will move %1 matching open window(s) to %2, workspace %3, and restore their saved layout and position. %4 missing window(s) will be skipped. No apps will be launched.")
                .arg(dialog.preview.matched || 0)
                .arg(dialog.preview.monitor || "")
                .arg(dialog.preview.workspace || "")
                .arg(dialog.preview.missing || 0)
        }
        onAccepted: {
            backend.openScene(dialog.selectedId)
            dialog.feedback = backend.status
            backend.refreshWindows()
            dialog.selectScene(dialog.selectedId)
        }
    }

    Dialog {
        id: removeConfirmation
        objectName: "removeSceneConfirmation"
        title: qsTr("Remove this saved scene?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(host.width - 80, 520)
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label {
            width: 460
            wrapMode: Text.WordWrap
            text: qsTr("Remove “%1”? This only deletes the saved scene; it does not change any open windows.")
                .arg(dialog.preview.name || "")
        }
        onAccepted: {
            backend.removeScene(dialog.selectedId)
            dialog.feedback = backend.status
            dialog.selectedId = ""
            dialog.preview = ({})
        }
    }
}
