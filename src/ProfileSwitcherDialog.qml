import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    objectName: "profileSwitcherDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 48, 760)
    height: Math.min(host.height - 48, 640)
    modal: true
    title: qsTr("Switch context profile")
    standardButtons: Dialog.Close

    property string selectedId: ""
    property string selectedName: ""
    property var review: ({})
    property string filterText: ""

    function choose(profile) {
        selectedId = profile.id
        selectedName = profile.name
        review = backend.reviewProfile(profile.id)
    }

    onOpened: {
        selectedId = ""
        selectedName = ""
        review = ({})
        filterText = ""
        profileSearch.text = ""
        profileSearch.forceActiveFocus()
    }

    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: contentHeight > availableHeight ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        ColumnLayout {
        width: scroller.availableWidth
        spacing: 10
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#aebbc3"
            text: qsTr("Review changes to saved rules and workspace pins before switching. Physical display settings are not changed.")
        }
        Label {
            Layout.fillWidth: true
            visible: dialog.host.dirty
            wrapMode: Text.WordWrap
            color: "#ffbd72"
            text: qsTr("Save or discard the unsaved rule draft before switching profiles.")
        }
        TextField {
            id: profileSearch
            objectName: "profileQuickSearch"
            Layout.fillWidth: true
            placeholderText: qsTr("Search profiles…")
            Accessible.name: qsTr("Search context profiles")
            onTextChanged: dialog.filterText = text.trim().toLocaleLowerCase()
        }
        ListView {
            id: profileList
            objectName: "profileQuickList"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(100, Math.min(contentHeight, 210))
            clip: true
            model: backend.profiles
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                visible: dialog.filterText.length === 0 ||
                    modelData.name.toLocaleLowerCase().includes(dialog.filterText)
                height: visible ? implicitHeight : 0
                highlighted: dialog.selectedId === modelData.id
                text: modelData.name + qsTr("  ·  %1 rules").arg(modelData.count)
                Accessible.name: qsTr("Preview profile %1 with %2 rules").arg(modelData.name).arg(modelData.count)
                onClicked: dialog.choose(modelData)
            }
            Label {
                anchors.centerIn: parent
                visible: backend.profiles.length === 0
                text: qsTr("No saved profiles yet. Create one in Presets & profiles.")
                color: "#aebbc3"
                wrapMode: Text.WordWrap
            }
        }
        Label {
            text: qsTr("Preview") + (dialog.selectedName ? " · " + dialog.selectedName : "")
            font.bold: true
        }
        ScrollView {
            objectName: "profileQuickPreview"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            TextArea {
                objectName: "profileQuickDiffText"
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                text: dialog.review.error || dialog.review.text || qsTr("Select a profile to see what would change.")
                Accessible.name: qsTr("Selected profile change preview")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: backend.status
                color: "#aebbc3"
                wrapMode: Text.WordWrap
            }
            Button {
                objectName: "profileQuickSwitchAction"
                text: qsTr("Review switch…")
                enabled: !!dialog.review.changed && !dialog.host.dirty && !backend.trialActive && !backend.monitorRevertPending
                onClicked: switchConfirmation.open()
            }
        }
        }
    }

    Dialog {
        id: switchConfirmation
        objectName: "profileQuickSwitchConfirmation"
        title: qsTr("Apply the reviewed profile changes?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(host.width - 64, 500)
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label {
            width: 440
            wrapMode: Text.WordWrap
            text: qsTr("Switch to “%1” and apply the rules and workspace assignments shown in the preview?")
                .arg(dialog.selectedName)
        }
        onAccepted: {
            if (!dialog.host.dirty && backend.applyProfileReview(dialog.selectedId, dialog.review.token))
                dialog.close()
            else dialog.review = backend.reviewProfile(dialog.selectedId)
        }
    }
}
