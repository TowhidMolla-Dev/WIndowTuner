import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    objectName: "workflowDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 60, 760)
    height: Math.min(host.height - 60, 640)
    modal: true
    title: qsTr("Presets & profiles")
    standardButtons: Dialog.Close
    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroller.availableWidth; spacing: 12
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Presets copy settings into your draft without changing its target. Profiles replace the active saved-rule collection after confirmation. They do not launch applications or run automatically.") }
            Label { visible: dialog.host.dirty; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#ffb778"; text: qsTr("Save or discard your current draft before activating a profile.") }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: presetName; objectName: "presetName"; Layout.fillWidth: true; placeholderText: qsTr("Custom preset name"); Accessible.name: qsTr("Custom preset name") }
                Button { objectName: "savePresetAction"; text: qsTr("Save draft as preset"); onClicked: backend.savePreset(presetName.text, dialog.host.options()) }
            }
            Repeater {
                model: backend.presets
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideRight }
                    Button { text: qsTr("Load into draft"); onClicked: { dialog.host.restorePreset(backend.presetSettings(modelData.id)); dialog.close() } }
                    Button { text: qsTr("Remove…"); onClicked: { removeConfirm.kind = "presets"; removeConfirm.itemId = modelData.id; removeConfirm.open() } }
                }
            }
            Label { text: qsTr("Profiles"); font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: profileName; Layout.fillWidth: true; placeholderText: qsTr("Work, Gaming, Presentation…"); Accessible.name: qsTr("Profile name") }
                Button { text: qsTr("Save current rules"); onClicked: backend.saveProfile(profileName.text) }
            }
            Repeater {
                model: backend.profiles
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: modelData.name + " · " + modelData.count; elide: Text.ElideRight }
                    Button { text: qsTr("Preview and activate…"); enabled: !dialog.host.dirty; onClicked: { profileConfirm.profileId = modelData.id; profileConfirm.preview = backend.profilePreview(modelData.id); profileConfirm.open() } }
                    Button { text: qsTr("Remove…"); onClicked: { removeConfirm.kind = "profiles"; removeConfirm.itemId = modelData.id; removeConfirm.open() } }
                }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: backend.status }
        }
    }
    StudioDialog {
        id: profileConfirm
        property string profileId
        property string preview
        anchors.centerIn: parent
        width: Math.min(dialog.width - 30, 650)
        height: Math.min(dialog.height - 30, 500)
        modal: true
        title: qsTr("Replace active rules with this profile?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ScrollView { clip: true; TextArea { readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; text: profileConfirm.preview } }
        onAccepted: backend.activateProfile(profileId)
    }
    StudioDialog {
        id: removeConfirm
        property string kind
        property string itemId
        anchors.centerIn: parent; modal: true
        title: qsTr("Remove this library item permanently?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: backend.removeLibraryItem(kind, itemId)
    }
}
