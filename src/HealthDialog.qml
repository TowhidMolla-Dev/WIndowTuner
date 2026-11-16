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
    objectName: "healthDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 60, 760)
    height: Math.min(host.height - 60, 650)
    modal: true
    title: qsTr("Compatibility & health")
    standardButtons: Dialog.Close
    onOpened: { report = backend.healthReport(); entryPath.text = backend.configEntryPoint }
    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        contentWidth: availableWidth
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
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Hyprland: %1\nSession reachable: %2\nModule exists: %3\nInclude present: %4\nExternal changes: %5\nLast successful save this session: %6").arg(dialog.report.version || qsTr("unknown")).arg(!!dialog.report.sessionReachable).arg(!!dialog.report.moduleExists).arg(!!dialog.report.includePresent).arg(!!dialog.report.externalChanges).arg(backend.lastSuccessfulSave || qsTr("none")) }
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
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Current configuration errors (may contain private paths):") }
            TextArea { Layout.fillWidth: true; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; text: dialog.report.configErrors || qsTr("None reported") }
            Button { objectName: "verifyDraftAction"; text: qsTr("Verify draft with installed Hyprland"); onClicked: dialog.verification = backend.verifyDraft(dialog.host.options()) }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: (dialog.verification.status || qsTr("Draft not verified yet")) + "\n" + (dialog.verification.coverage || "") }
            TextArea { Layout.fillWidth: true; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; text: dialog.verification.details || ""; visible: text.length > 0 }
            Button { text: qsTr("Generate private-data-free diagnostics"); onClicked: diagnostics.text = backend.diagnosticReport() }
            TextArea { id: diagnostics; Layout.fillWidth: true; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; placeholderText: qsTr("Review the report here before copying it.") }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: backend.status }
        }
    }
}
