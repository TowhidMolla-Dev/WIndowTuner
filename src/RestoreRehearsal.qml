import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StudioDialog {
    id: rehearsal
    required property var report
    required property var host
    property string summaryObjectName
    property var selection: []
    signal applySelection(var tokens)
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(host.width - 48, 690)
    height: Math.min(host.height - 48, 640)
    modal: true
    onAboutToShow: {
        selection = []
        rows.contentY = 0
    }

    function select(token, checked) {
        selection = checked ? selection.concat([token]) : selection.filter(item => item !== token)
    }

    contentItem: ColumnLayout {
        Label {
            objectName: rehearsal.summaryObjectName
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: rehearsal.report.error || qsTr("Choose which windows to restore. Unchecked and unrelated windows stay where they are. No apps will be launched. This cannot be automatically undone.")
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("%1 selected · %2 missing · %3 unrelated open windows stay")
                .arg(rehearsal.selection.length).arg(rehearsal.report.missing || 0)
                .arg(rehearsal.report.otherWindows || 0)
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ListView {
                id: rows
                spacing: 8
                model: rehearsal.report.items || []
                delegate: Rectangle {
                    id: card
                    required property var modelData
                    required property int index
                    readonly property bool selected: rehearsal.selection.includes(modelData.token)
                    width: rows.width
                    height: details.implicitHeight + 20
                    color: "#15212a"
                    radius: 7
                    ColumnLayout {
                        id: details
                        anchors.fill: parent
                        anchors.margins: 10
                        CheckBox {
                            objectName: "restoreWindowChoice" + card.index
                            Layout.fillWidth: true
                            visible: card.modelData.matched
                            text: qsTr("%1 · %2").arg(card.index + 1).arg(card.modelData["class"])
                            checked: card.selected
                            onToggled: rehearsal.select(card.modelData.token, checked)
                        }
                        Label {
                            visible: !card.modelData.matched
                            text: qsTr("%1 · Missing; will not be launched").arg(card.modelData["class"])
                            color: "#ffbd72"
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: card.modelData.matched
                            DisplaySketch {
                                caption: qsTr("Before · %1 / workspace %2")
                                    .arg(card.modelData.currentMonitor || "—").arg(card.modelData.currentWorkspace || "—")
                                geometry: card.modelData.before
                            }
                            DisplaySketch {
                                caption: card.selected
                                    ? qsTr("After · %1 / workspace %2").arg(card.modelData.targetMonitor).arg(card.modelData.targetWorkspace)
                                    : qsTr("After · stays where it is")
                                geometry: card.selected ? card.modelData.after : card.modelData.before
                                proposed: card.selected
                                approximate: card.selected && !card.modelData.floating
                            }
                        }
                    }
                }
            }
        }
    }

    footer: DialogButtonBox {
        Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        Button {
            objectName: "applyConfirmedWindows"
            text: qsTr("Restore selected (%1)").arg(rehearsal.selection.length)
            enabled: rehearsal.selection.length > 0 && !rehearsal.report.error
                && !rehearsal.host.dirty && !backend.trialActive && !backend.monitorRevertPending
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
    onAccepted: if (selection.length) applySelection(selection)

    component DisplaySketch: ColumnLayout {
        id: sketch
        property string caption
        property var geometry
        property bool proposed: false
        property bool approximate: false
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Label { Layout.fillWidth: true; text: sketch.caption; wrapMode: Text.Wrap }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            clip: true
            radius: 4
            color: "#0e171d"
            border.color: "#526572"
            Rectangle {
                visible: !!sketch.geometry
                x: sketch.geometry ? sketch.geometry.x * parent.width : 0
                y: sketch.geometry ? sketch.geometry.y * parent.height : 0
                width: sketch.geometry ? sketch.geometry.width * parent.width : 0
                height: sketch.geometry ? sketch.geometry.height * parent.height : 0
                color: sketch.proposed ? "#70b68a" : "#647e91"
                radius: 3
            }
            Label { anchors.centerIn: parent; visible: !sketch.geometry; text: qsTr("Geometry unavailable") }
        }
        Label { visible: sketch.approximate; text: qsTr("Tiled placement is approximate"); font.pixelSize: 10 }
    }
}
