import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    property int step: 0
    objectName: "welcomeDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 60, 620)
    modal: true
    title: qsTr("Welcome to WindowTuner")
    closePolicy: Popup.NoAutoClose
    contentItem: ColumnLayout {
        spacing: 16
        Label { text: qsTr("Step %1 of 4").arg(dialog.step + 1); color: theme.accent }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: [
                qsTr("1. Choose a window on the Windows page, or open Tools & workflows → Applications & imports to configure an app that is not running."),
                qsTr("2. Use Layout, Placement, Appearance and Behavior. Unchecked overrides mean unchanged, not force-off. Identical app/title windows can share a rule. Every ⓘ button explains its module."),
                qsTr("3. Test the draft match with the inspector. Review the plain-language summary and before/after diff before saving. Installed-parser verification does not change your desktop."),
                qsTr("4. Recover using Safety & history. Save reusable effects as presets, or collections as profiles. Ctrl+S reviews a save; Ctrl+K searches; Alt+1–5 changes section; ? shows shortcuts.")
            ][dialog.step]
        }
        RowLayout {
            Layout.fillWidth: true
            Button { text: qsTr("Back"); enabled: dialog.step > 0; onClicked: --dialog.step }
            Item { Layout.fillWidth: true }
            Button {
                objectName: "welcomeNext"
                text: dialog.step === 3 ? qsTr("Get started") : qsTr("Next")
                onClicked: {
                    if (dialog.step < 3) ++dialog.step
                    else if (backend.setPreference("onboarded", true)) dialog.close()
                }
            }
        }
    }
}
