import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    property var report: ({})
    property var verification: ({})
    property var featureReport: []
    property bool showConfigErrors: false
    property bool showVerificationDetails: false
    objectName: "healthDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 60, 760)
    height: Math.min(host.height - 60, Math.max(420, implicitHeight))
    modal: true
    title: qsTr("Compatibility & health")
    standardButtons: Dialog.Close
    onOpened: { report = backend.healthReport(); entryPath.text = backend.configEntryPoint }
    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        rightPadding: 18
        contentWidth: availableWidth
        ScrollBar.vertical.policy: contentHeight > availableHeight ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        clip: true
        ColumnLayout {
            width: scroller.availableWidth; spacing: 12
            Button { objectName: "refreshHealthAction"; text: qsTr("Refresh health"); onClicked: dialog.report = backend.healthReport() }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Interface scale") }
                Slider {
                    objectName: "interfaceScale"
                    Accessible.name: qsTr("Interface scale")
                    Layout.fillWidth: true; from: 0.85; to: 1.4; stepSize: 0.05; value: dialog.host.textScale
                    onMoved: { dialog.host.textScale = value; backend.setPreference("textScale", value) }
                }
                Label { text: qsTr("%1%").arg(Math.round(dialog.host.textScale * 100)) }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Larger interface scales increase the minimum window size. Dialogs remain scrollable.") }
            Label { text: qsTr("Session & configuration"); font.bold: true }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 20
                rowSpacing: 6
                Repeater {
                    model: [
                        { label: qsTr("Hyprland"), value: dialog.report.version || qsTr("unknown") },
                        { label: qsTr("Session reachable"), value: dialog.report.sessionReachable ? qsTr("Yes") : qsTr("No") },
                        { label: qsTr("Module exists"), value: dialog.report.moduleExists ? qsTr("Yes") : qsTr("No") },
                        { label: qsTr("Include present"), value: dialog.report.includePresent ? qsTr("Yes") : qsTr("No") },
                        { label: qsTr("External changes"), value: dialog.report.externalChanges ? qsTr("Yes") : qsTr("No") },
                        { label: qsTr("Last successful save"), value: backend.lastSuccessfulSave || qsTr("None this session") }
                    ]
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8
                        Label { text: modelData.label; color: "#93a4b3"; Layout.minimumWidth: 110 }
                        Label { text: modelData.value; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
                    }
                }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: dialog.report.capabilityStatus || "" }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: dialog.report.coverage || "" }
            Button { text: qsTr("Audit support for every feature group"); onClicked: dialog.featureReport = backend.auditCapabilities() }
            Repeater {
                model: dialog.featureReport
                delegate: Label {
                    required property var modelData
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: modelData.name + " · " + modelData.status
                    ToolTip.visible: featureHover.hovered
                    ToolTip.text: modelData.details
                    HoverHandler { id: featureHover }
                }
            }
            TextField { id: entryPath; objectName: "entryPointPath"; Layout.fillWidth: true; placeholderText: qsTr("Absolute main Lua config path"); Accessible.name: qsTr("Main Lua configuration path") }
            Button { text: qsTr("Use this entry point"); enabled: !dialog.host.dirty; onClicked: { backend.configureEntryPoint(entryPath.text); dialog.report = backend.healthReport() } }
            Button {
                objectName: "toggleConfigErrorsAction"
                checkable: true
                checked: dialog.showConfigErrors
                text: dialog.showConfigErrors
                    ? qsTr("Hide configuration errors")
                    : qsTr("Show configuration errors (may contain private paths)")
                onClicked: dialog.showConfigErrors = checked
            }
            TextArea {
                objectName: "configErrorsDetails"
                visible: dialog.showConfigErrors
                Layout.fillWidth: true
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                text: dialog.report.configErrors || qsTr("None reported")
            }
            Label { text: qsTr("Draft verification"); font.bold: true }
            Button { objectName: "verifyDraftAction"; text: qsTr("Verify draft with installed Hyprland"); onClicked: dialog.verification = backend.verifyDraft(dialog.host.options()) }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: (dialog.verification.status || qsTr("Draft not verified yet")) + "\n" + (dialog.verification.coverage || "") }
            Button {
                objectName: "toggleVerificationDetailsAction"
                visible: (dialog.verification.details || "").length > 0
                checkable: true
                checked: dialog.showVerificationDetails
                text: dialog.showVerificationDetails ? qsTr("Hide verification details") : qsTr("Show verification details")
                onClicked: dialog.showVerificationDetails = checked
            }
            TextArea {
                objectName: "verificationDetails"
                Layout.fillWidth: true
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                text: dialog.verification.details || ""
                visible: dialog.showVerificationDetails && text.length > 0
            }
            Label { text: qsTr("Shareable diagnostics"); font.bold: true }
            Button { text: qsTr("Generate private-data-free diagnostics"); onClicked: diagnostics.text = backend.diagnosticReport() }
            TextArea {
                id: diagnostics
                objectName: "shareableDiagnostics"
                Layout.fillWidth: true
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                placeholderText: qsTr("Review the report here before copying it.")
                visible: text.length > 0
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: backend.status }
        }
    }
}
