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
    property bool showWindowProperties: false
    property bool showEvaluationDetails: false
    objectName: "inspectorDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 60, 760)
    height: Math.min(host.height - 60, 640)
    modal: true
    title: qsTr("Effective settings inspector")
    standardButtons: Dialog.Close
    function refresh() { report = backend.inspectWindow(); matchReport = backend.testMatch(host.options()) }
    onOpened: refresh()
    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        rightPadding: 18
        contentWidth: availableWidth
        ScrollBar.vertical.policy: contentHeight > availableHeight ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        clip: true
        ColumnLayout {
            width: scroller.availableWidth; spacing: 12
            Button { objectName: "testMatchAction"; text: qsTr("Refresh inspector & test draft match"); onClicked: dialog.refresh() }
            Label { text: qsTr("Observed now"); font.bold: true }
            Label {
                objectName: "inspectorObservedWindow"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: {
                    const w = dialog.report.window || {}
                    if (!w.address) return qsTr("Select an open window to inspect live properties. Offline targets have no observed state.")
                    const layout = w.floating === undefined ? qsTr("unknown layout")
                        : w.floating ? qsTr("floating") : qsTr("tiled")
                    return qsTr("%1 · %2 · workspace %3\nSize: %4 · Position: %5\nEffective opacity, borders and rule precedence are not reported here.")
                        .arg(w.currentClass || w["class"] || qsTr("unknown class")).arg(layout)
                        .arg(w.workspace || qsTr("unknown")).arg((w.size || []).join(" × ") || qsTr("unknown"))
                        .arg((w.at || []).join(", ") || qsTr("unknown"))
                }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Draft matches %1 open window(s). Unknown: %2.").arg(dialog.matchReport.count || 0).arg((dialog.matchReport.uncertain || []).length); font.bold: true }
            Repeater {
                model: dialog.matchReport.matches || []
                delegate: Label { required property var modelData; Layout.fillWidth: true; text: modelData.window["class"] + " · " + modelData.window.currentTitle; elide: Text.ElideRight }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Local match estimate; effective compositor precedence is not guaranteed.")
            }
            Label { text: qsTr("Configured by matching WindowTuner rules"); font.bold: true }
            Repeater {
                model: dialog.report.rules || []
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: ruleDetails.implicitHeight + 16
                    radius: 6
                    color: "#182128"
                    border.color: modelData.unknown ? "#806744" : "#34434c"
                    ColumnLayout {
                        id: ruleDetails
                        anchors.fill: parent
                        anchors.margins: 8
                        Label { Layout.fillWidth: true; text: modelData.name + (modelData.unknown ? qsTr(" · uncertain match") : ""); font.bold: true }
                        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: modelData.summary; color: "#c2cdd6" }
                    }
                }
            }
            Label { text: qsTr("Rule conflict map"); font.bold: true }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#aebbc3"
                text: qsTr("Each link compares requested values from two rules that match this window. It does not predict Hyprland’s final precedence.")
            }
            Repeater {
                objectName: "ruleConflictMap"
                model: dialog.report.conflictMap || []
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: conflictDetails.implicitHeight + 16
                    radius: 7
                    color: "#251d18"
                    border.color: "#9d633b"
                    RowLayout {
                        id: conflictDetails
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: modelData.firstRule; font.bold: true; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; text: modelData.firstValue; color: "#ffbd72"; wrapMode: Text.WordWrap }
                        }
                        Label { text: qsTr("%1 differs").arg(modelData.setting); color: "#ffbd72"; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: modelData.secondRule; font.bold: true; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; text: modelData.secondValue; color: "#ffbd72"; wrapMode: Text.WordWrap }
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                visible: (dialog.report.conflictMap || []).length === 0
                wrapMode: Text.WordWrap
                color: "#8bd5a5"
                text: qsTr("No differing requested values found among the rules that can be evaluated for this window.")
            }
            Button {
                objectName: "toggleEvaluationDetailsAction"
                checkable: true
                checked: dialog.showEvaluationDetails
                text: dialog.showEvaluationDetails
                    ? qsTr("Hide evaluation limits and config hints")
                    : qsTr("Show evaluation limits and config hints")
                onClicked: dialog.showEvaluationDetails = checked
            }
            ColumnLayout {
                objectName: "evaluationDetails"
                visible: dialog.showEvaluationDetails
                Layout.fillWidth: true
                spacing: 8
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: dialog.matchReport.coverage || "" }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: dialog.report.coverage || "" }
                Label { text: qsTr("Possible external rules and includes"); font.bold: true }
                Repeater {
                    model: dialog.report.externalHints || []
                    delegate: Label { required property var modelData; Layout.fillWidth: true; text: qsTr("Main config line %1: %2").arg(modelData.line).arg(modelData.kind); wrapMode: Text.WordWrap }
                }
            }
            Button {
                objectName: "toggleWindowPropertiesAction"
                checkable: true
                checked: dialog.showWindowProperties
                text: dialog.showWindowProperties
                    ? qsTr("Hide window properties (JSON)")
                    : qsTr("Show window properties (JSON)")
                onClicked: dialog.showWindowProperties = checked
            }
            ScrollView {
                id: propertiesScroll
                objectName: "windowPropertiesDetails"
                visible: dialog.showWindowProperties
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                contentWidth: availableWidth
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                TextArea {
                    Accessible.name: qsTr("Current window properties as JSON")
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    text: JSON.stringify(dialog.report.window || {}, null, 2)
                    font.pixelSize: 11
                }
            }
        }
    }
}
