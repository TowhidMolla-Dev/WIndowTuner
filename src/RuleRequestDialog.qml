import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StudioDialog {
    id: dialog
    objectName: "ruleRequestDialog"
    required property var host
    property var interpretation: ({})
    title: qsTr("Describe a window rule")
    anchors.centerIn: parent
    width: Math.min(host.width - 48, 680)
    height: Math.min(host.height - 48, 520)
    modal: true
    standardButtons: Dialog.Close
    onOpened: {
        requestField.text = ""
        interpretation = ({})
        requestField.forceActiveFocus()
    }

    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroller.availableWidth
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("“This app” means the selected window’s exact current class: %1. The rule will target all windows of that class.")
                    .arg(backend.selectedWindow.address && backend.selectedWindow.currentClass || qsTr("no open window selected"))
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Try “open this app floating at 800×600 on workspace 3” or “open this app tiled on workspace 2”. Supports English layout, size, centering, and numbered workspaces. Other wording is rejected rather than guessed.")
            }
            TextField {
                id: requestField
                objectName: "ruleRequestInput"
                Layout.fillWidth: true
                placeholderText: qsTr("Open this app…")
                Accessible.name: qsTr("Describe the rule for the selected app")
                onTextChanged: dialog.interpretation = ({})
                onAccepted: if (parse.enabled) parse.clicked()
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    id: parse
                    objectName: "interpretRuleRequest"
                    text: qsTr("Interpret request")
                    enabled: requestField.text.trim() !== ""
                    onClicked: dialog.interpretation = backend.interpretRuleRequest(requestField.text)
                }
                Button {
                    objectName: "stageRuleRequest"
                    text: qsTr("Open as editable draft")
                    enabled: !!dialog.interpretation.settings && !dialog.host.dirty
                        && dialog.interpretation.address === backend.selectedWindow.address
                        && dialog.interpretation["class"] === backend.selectedWindow.currentClass
                    onClicked: {
                        dialog.host.stageRuleRequest(dialog.interpretation.settings)
                        dialog.close()
                    }
                }
            }
            Label {
                objectName: "ruleRequestResult"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: dialog.interpretation.error ? "#ffbd72" : palette.text
                text: dialog.interpretation.error || dialog.interpretation.summary
                    || qsTr("Interpreting a request does not change the editor or save a rule.")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: !!dialog.interpretation.settings
                text: (dialog.interpretation.usesLiveSize ? qsTr("No size specified: the observed window size was used. ") : "")
                    + qsTr("The draft starts with only these requested settings. Review the editor preview and confirm the normal save review to apply it.")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: dialog.host.dirty
                text: qsTr("Save or discard the current editor draft before opening a new one.")
            }
        }
    }
}
