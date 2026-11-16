import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    property var applications: []
    property var selectedIds: []
    objectName: "managementDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 60, 780)
    height: Math.min(host.height - 60, 650)
    modal: true
    title: qsTr("Applications & rule management")
    standardButtons: Dialog.Close
    onOpened: { applications = backend.installedApplications(); selectedIds = [] }
    function selectId(id, checked) {
        const copy = selectedIds.slice()
        const at = copy.indexOf(id)
        if (checked && at < 0) copy.push(id)
        else if (!checked && at >= 0) copy.splice(at, 1)
        selectedIds = copy
    }
    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroller.availableWidth; spacing: 12
            Label { text: qsTr("Create a rule before opening an application"); font.bold: true }
            Label { visible: dialog.host.dirty; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#ffb778"; text: qsTr("Save or discard the current draft before selecting a different target.") }
            ComboBox {
                id: appChoice; Layout.fillWidth: true; model: dialog.applications; textRole: "name"
                onActivated: { offlineClass.text = dialog.applications[currentIndex]["class"]; offlineTitle.text = "" }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Class suggestions come from desktop entries and can be wrong. Confirm the actual class/app ID after opening the application. Nothing here launches a command.") }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: offlineClass; objectName: "offlineClass"; Layout.fillWidth: true; placeholderText: qsTr("Window class / Wayland app ID"); Accessible.name: qsTr("Offline target class") }
                TextField { id: offlineTitle; Layout.fillWidth: true; placeholderText: qsTr("Optional exact title"); Accessible.name: qsTr("Offline target title") }
            }
            Button {
                objectName: "offlineTargetAction"
                text: qsTr("Configure offline target")
                enabled: !dialog.host.dirty
                onClicked: {
                    if (!backend.selectOfflineTarget(offlineClass.text, offlineTitle.text)) return
                    dialog.host.startOfflineTarget(offlineTitle.text.length > 0)
                    dialog.close()
                }
            }
            Label { text: qsTr("Batch actions & organization"); font.bold: true }
            Flow {
                Layout.fillWidth: true; spacing: 8
                Button { text: qsTr("Enable selected"); enabled: dialog.selectedIds.length > 0; onClicked: backend.batchSetEnabled(dialog.selectedIds, true) }
                Button { text: qsTr("Disable selected"); enabled: dialog.selectedIds.length > 0; onClicked: backend.batchSetEnabled(dialog.selectedIds, false) }
                Button { text: qsTr("Export selected"); enabled: dialog.selectedIds.length > 0; onClicked: { exportText.text = backend.exportSelected(dialog.selectedIds); exportDialog.open() } }
                Button { text: qsTr("Selective import…"); onClicked: importDialog.open() }
            }
            Repeater {
                model: backend.savedRules
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    CheckBox { Accessible.name: qsTr("Select %1").arg(modelData.name); checked: dialog.selectedIds.indexOf(modelData.id) >= 0; onToggled: dialog.selectId(modelData.id, checked) }
                    Label { Layout.fillWidth: true; text: (modelData.folder || qsTr("Unfiled")) + " / " + modelData.name + " · " + (modelData.tags || []).join(", "); elide: Text.ElideRight }
                    Button { text: qsTr("Organize…"); onClicked: { metadataDialog.ruleId = modelData.id; ruleName.text = modelData.name; ruleFolder.text = modelData.folder || ""; ruleTags.text = (modelData.tags || []).join(", "); metadataDialog.open() } }
                }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: backend.status }
        }
    }
    StudioDialog {
        id: metadataDialog
        property string ruleId
        anchors.centerIn: parent; width: Math.min(dialog.width - 40, 560); modal: true
        title: qsTr("Rule name, folder & tags")
        standardButtons: Dialog.Save | Dialog.Cancel
        ColumnLayout {
            width: parent.width
            TextField { id: ruleName; Layout.fillWidth: true; placeholderText: qsTr("Name"); Accessible.name: qsTr("Rule name") }
            TextField { id: ruleFolder; Layout.fillWidth: true; placeholderText: qsTr("Folder"); Accessible.name: qsTr("Rule folder") }
            TextField { id: ruleTags; Layout.fillWidth: true; placeholderText: qsTr("Tags, separated, by commas"); Accessible.name: qsTr("Rule tags") }
        }
        onAccepted: backend.setRuleMetadata(ruleId, ruleName.text, ruleFolder.text, ruleTags.text)
    }
    StudioDialog {
        id: exportDialog
        anchors.centerIn: parent; width: Math.min(dialog.width - 40, 660); height: 400; modal: true
        title: qsTr("Selected rules · copy JSON")
        standardButtons: Dialog.Close
        contentItem: ScrollView { clip: true; TextArea { id: exportText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere } }
    }
    StudioDialog {
        id: importDialog
        property var preview: ({})
        property var selected: []
        anchors.centerIn: parent; width: Math.min(dialog.width - 30, 700); height: Math.min(dialog.height - 30, 590); modal: true
        title: qsTr("Selective import")
        standardButtons: Dialog.Cancel
        contentItem: ScrollView {
            id: importScroller
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: importScroller.availableWidth; spacing: 8
                TextArea { id: importText; Layout.fillWidth: true; Layout.preferredHeight: 150; wrapMode: TextEdit.WrapAnywhere; placeholderText: qsTr("Paste a WindowTuner JSON rule array"); onTextChanged: { importDialog.preview = ({}); importDialog.selected = [] } }
                Button { objectName: "previewImportAction"; text: qsTr("Validate & preview"); onClicked: { importDialog.preview = backend.previewImport(importText.text); importDialog.selected = [] } }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: importDialog.preview.error || qsTr("Select incoming rules. Merge adds copies; Replace removes every existing active rule not included in this import.") }
                Repeater {
                    model: importDialog.preview.rules || []
                    delegate: CheckBox {
                        required property var modelData
                        text: modelData.name + " · " + modelData["class"]
                        onToggled: {
                            const copy = importDialog.selected.slice()
                            if (checked) copy.push(modelData.index)
                            else copy.splice(copy.indexOf(modelData.index), 1)
                            importDialog.selected = copy
                        }
                    }
                }
                ComboBox { id: importMode; model: [qsTr("Merge (keep existing)"), qsTr("Replace entire collection")]; Layout.fillWidth: true }
                Button { text: qsTr("Review import…"); enabled: importDialog.selected.length > 0; onClicked: importConfirm.open() }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: backend.status }
            }
        }
        StudioDialog {
            id: importConfirm
            anchors.centerIn: parent; modal: true
            title: importMode.currentIndex === 0 ? qsTr("Merge selected rules?") : qsTr("Replace ALL current rules?")
            standardButtons: Dialog.Ok | Dialog.Cancel
            Label { width: 320; wrapMode: Text.WordWrap; text: qsTr("%1 incoming rule(s) selected. Undo will be available after a successful import.").arg(importDialog.selected.length) }
            onAccepted: if (backend.importSelection(importText.text, importDialog.selected, importMode.currentIndex === 0 ? "merge" : "replace")) importDialog.close()
        }
    }
}
