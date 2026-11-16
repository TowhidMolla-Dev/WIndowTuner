import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    property var report: ({})
    property var matchReport: ({})
    objectName: "inspectorDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 60, 760)
    height: Math.min(host.height - 60, 640)
    modal: true
    title: qsTr("Why is this window behaving this way?")
    standardButtons: Dialog.Close
    function refresh() { report = backend.inspectWindow(); matchReport = backend.testMatch(host.options()) }
    onOpened: refresh()
    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroller.availableWidth; spacing: 12
            Button { objectName: "testMatchAction"; text: qsTr("Refresh inspector & test draft match"); onClicked: dialog.refresh() }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Draft matches %1 open window(s). Unknown: %2.").arg(dialog.matchReport.count || 0).arg((dialog.matchReport.uncertain || []).length); font.bold: true }
            Repeater {
                model: dialog.matchReport.matches || []
                delegate: Label { required property var modelData; Layout.fillWidth: true; text: modelData.window["class"] + " · " + modelData.window.currentTitle; elide: Text.ElideRight }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: dialog.matchReport.coverage || "" }
            Label { text: qsTr("Current properties"); font.bold: true }
            TextArea { Layout.fillWidth: true; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; text: JSON.stringify(dialog.report.window || {}, null, 2); font.pixelSize: 11 }
            Label { text: qsTr("Matching enabled rules"); font.bold: true }
            Repeater {
                model: dialog.report.rules || []
                delegate: Label { required property var modelData; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: modelData.name + (modelData.unknown ? qsTr(" · uncertain") : "") + "\n" + modelData.summary }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#ffb778"; text: (dialog.report.conflicts || []).join("\n") }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: dialog.report.coverage || "" }
            Repeater {
                model: dialog.report.externalHints || []
                delegate: Label { required property var modelData; Layout.fillWidth: true; text: qsTr("Main config line %1: %2").arg(modelData.line).arg(modelData.kind); wrapMode: Text.WordWrap }
            }
        }
    }
}
