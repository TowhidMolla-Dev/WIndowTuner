import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StudioDialog {
    id: dialog
    objectName: "classDiscoveryDialog"
    required property var host
    title: qsTr("Discover an app’s window class")
    anchors.centerIn: parent
    width: Math.min(host.width - 48, 720)
    height: Math.min(host.height - 48, 650)
    modal: true
    standardButtons: Dialog.Close

    property var apps: []
    property string windowAddress: ""
    property var selectedHint: null
    property bool tested: false
    readonly property var report: {
        if (!tested) return null
        // Keep results current when windows actually open or close.
        const windows = backend.windows
        const key = classField.currentIndex === 1 ? "initial_class" : "class"
        return backend.testMatch({match: {[key]: draftClass.text.trim()}, classMatchMode: "exact"})
    }
    readonly property var observed: backend.windows.find(window => window.address === windowAddress) || null
    readonly property var hints: {
        const needle = hintFilter.text.trim().toLocaleLowerCase()
        return apps.filter(app => !needle || (app.name + " " + app["class"]).toLocaleLowerCase().includes(needle))
            .map(app => Object.assign({}, app, {label: app.name + " · " + app["class"]}))
    }

    function useObservedClass() {
        draftClass.text = observed ? (classField.currentIndex === 1
            ? observed["class"] : observed.currentClass) || "" : ""
        tested = false
    }

    onAboutToShow: {
        apps = backend.installedApplications()
        windowAddress = backend.selectedWindow.address || ""
        hintFilter.text = ""
        hintPicker.currentIndex = -1
        selectedHint = null
        classField.currentIndex = 0
        useObservedClass()
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
                text: qsTr("Open the app normally, then select its window below. Desktop entries are clues, not proof of the class Hyprland sees.")
            }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    objectName: "discoveryWindowPicker"
                    Layout.fillWidth: true
                    model: backend.windows.map(window => ({address: window.address,
                        label: window.currentTitle || window.currentClass || window["class"]}))
                    textRole: "label"
                    currentIndex: backend.windows.findIndex(window => window.address === dialog.windowAddress)
                    displayText: currentIndex < 0 ? qsTr("Select an open window") : currentText
                    Accessible.name: qsTr("Open window to observe")
                    onActivated: {
                        dialog.windowAddress = model[currentIndex].address
                        dialog.useObservedClass()
                    }
                }
                Button {
                    text: qsTr("Refresh windows")
                    onClicked: backend.refreshWindows()
                }
            }
            Label {
                objectName: "discoveryObservedClass"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: dialog.observed
                    ? qsTr("Observed by Hyprland\nCurrent class: %1 · Initial class: %2 · %3")
                        .arg(dialog.observed.currentClass || qsTr("unavailable"))
                        .arg(dialog.observed["class"] || qsTr("unavailable"))
                        .arg(dialog.observed.xwayland ? qsTr("XWayland") : qsTr("Wayland"))
                    : qsTr("No live window selected. Open the app and refresh to observe its class.")
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: dialog.host.outline }
            TextField {
                id: hintFilter
                objectName: "discoveryHintSearch"
                Layout.fillWidth: true
                placeholderText: qsTr("Find an installed app to compare…")
                Accessible.name: qsTr("Search installed app hints")
                onTextChanged: { hintPicker.currentIndex = -1; dialog.selectedHint = null }
            }
            ComboBox {
                id: hintPicker
                objectName: "discoveryHintPicker"
                Layout.fillWidth: true
                model: dialog.hints
                textRole: "label"
                currentIndex: -1
                displayText: currentIndex < 0 ? qsTr("Select a desktop entry (optional)") : currentText
                Accessible.name: qsTr("Desktop entry to compare")
                onActivated: dialog.selectedHint = dialog.hints[currentIndex]
            }
            Label {
                objectName: "discoveryHintClass"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: {
                    const hint = dialog.selectedHint
                    if (!hint) return qsTr("Desktop-entry hints are unverified.")
                    const source = hint.inferred ? qsTr("Filename guess") : qsTr("Declared StartupWMClass")
                    const comparison = !dialog.observed ? "" : hint["class"] === dialog.observed.currentClass
                        ? qsTr(" · Same text as the observed current class")
                        : qsTr(" · Differs from the observed current class")
                    return qsTr("%1 (unverified): %2%3").arg(source).arg(hint["class"]).arg(comparison)
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: dialog.host.outline }
            Label { text: qsTr("Test an exact draft match"); font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: classField
                    objectName: "discoveryClassField"
                    model: [qsTr("Current class"), qsTr("Initial class")]
                    Accessible.name: qsTr("Draft match field")
                    onActivated: dialog.useObservedClass()
                }
                TextField {
                    id: draftClass
                    objectName: "discoveryDraftClass"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Class to test")
                    Accessible.name: qsTr("Exact class to test")
                    onTextChanged: dialog.tested = false
                }
                Button {
                    objectName: "testDiscoveredClass"
                    text: qsTr("Test match")
                    enabled: draftClass.text.trim() !== ""
                    onClicked: dialog.tested = true
                }
            }
            Label {
                objectName: "discoveryResult"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: dialog.report
                    ? qsTr("%1 of %2 open windows match this exact draft. Nothing was saved.")
                        .arg(dialog.report.count).arg(backend.windows.length)
                    : qsTr("This test checks open windows only and does not change the editor or saved rules.")
            }
            Repeater {
                model: dialog.report ? (dialog.report.matches || []).concat(dialog.report.uncertain || []) : []
                delegate: Label {
                    required property var modelData
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: qsTr("%1 — %2").arg(modelData.window.currentTitle || modelData.window.currentClass
                        || modelData.window["class"]).arg(modelData.reason)
                }
            }
        }
    }
}
