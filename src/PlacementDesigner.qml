import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    property var placement: ({ enabled: false, runtimeRecovery: true, unit: "percent", width: 60, height: 60, anchor: "center", margin: 24, aspect: 1.5 })
    property var geometry: { const inventory = backend.monitors; return backend.resolvePlacement({placement: placement}) }
    function change(key, value) {
        const copy = Object.assign({}, placement); copy[key] = value; placement = copy
    }
    objectName: "placementDesigner"
    anchors.centerIn: parent
    width: Math.min(host.width - 60, 760)
    height: Math.min(host.height - 60, 640)
    modal: true
    title: qsTr("Visual placement designer")
    standardButtons: Dialog.Close
    contentItem: ScrollView {
        id: scroller
        objectName: "toolScroll"
        rightPadding: 18
        contentWidth: availableWidth
        ScrollBar.vertical.policy: contentHeight > availableHeight ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        clip: true
        ColumnLayout {
            width: scroller.availableWidth; spacing: 10
            CheckBox { objectName: "placementEnabled"; text: qsTr("Use designer geometry instead of Layout size/position"); checked: dialog.placement.enabled; onToggled: dialog.change("enabled", checked) }
            CheckBox { objectName: "runtimeRecovery"; text: qsTr("Recover automatically on monitor disconnect and reconnect"); checked: !!dialog.placement.runtimeRecovery; enabled: !!dialog.placement.enabled; onToggled: dialog.change("runtimeRecovery", checked) }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Drag to position; drag the corner to resize. Save to install recovery inside Hyprland—it works with this app closed. Recovery recalculates percentages on connected outputs and returns untouched windows when the preferred monitor reconnects. Manual changes, fullscreen/tiled/grouped windows and conflicting rules are left alone.") }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Target") }
                ComboBox { id: monitorChoice; Layout.fillWidth: true; model: backend.monitors; textRole: "name"; currentIndex: backend.monitors.findIndex(m => m.name === dialog.placement.monitor); displayText: currentIndex < 0 ? qsTr("Automatic") : currentText; onActivated: dialog.change("monitor", currentText) }
                Label { text: qsTr("Fallback") }
                ComboBox { id: fallbackChoice; Layout.fillWidth: true; model: backend.monitors; textRole: "name"; currentIndex: backend.monitors.findIndex(m => m.name === dialog.placement.fallback); displayText: currentIndex < 0 ? qsTr("First available") : currentText; onActivated: dialog.change("fallback", currentText) }
            }
            Flow {
                Layout.fillWidth: true; spacing: 6
                Repeater {
                    model: backend.monitors
                    delegate: Button { required property var modelData; text: modelData.name + " · " + modelData.width + "×" + modelData.height; onClicked: dialog.change("monitor", modelData.name) }
                }
            }
            Rectangle {
                id: canvas
                objectName: "placementCanvas"
                Layout.fillWidth: true
                Layout.preferredHeight: 210
                color: "#0b141b"; border.color: "#39505f"; clip: true
                readonly property real zoom: Math.min((width - 20) / (dialog.geometry.monitorWidth || 1920), (height - 20) / (dialog.geometry.monitorHeight || 1080))
                Rectangle {
                    id: screenPreview
                    anchors.centerIn: parent
                    width: (dialog.geometry.monitorWidth || 1920) * canvas.zoom
                    height: (dialog.geometry.monitorHeight || 1080) * canvas.zoom
                    color: "#192630"; border.color: "#7c919f"
                    Rectangle {
                        x: (dialog.geometry.usableX || 0) * canvas.zoom
                        y: (dialog.geometry.usableY || 0) * canvas.zoom
                        width: (dialog.geometry.usableWidth || 1920) * canvas.zoom
                        height: (dialog.geometry.usableHeight || 1080) * canvas.zoom
                        color: "#101b22"; border.color: "#425867"
                    }
                    Rectangle {
                        id: windowPreview
                        objectName: "placementWindow"
                        x: (dialog.geometry.x || 0) * canvas.zoom; y: (dialog.geometry.y || 0) * canvas.zoom
                        width: (dialog.geometry.width || 600) * canvas.zoom; height: (dialog.geometry.height || 420) * canvas.zoom
                        color: Qt.alpha(theme.accent, 0.25); border.color: theme.accent
                        Label { anchors.centerIn: parent; text: qsTr("Window"); font.pixelSize: 11 }
                        MouseArea {
                            anchors.fill: parent; preventStealing: true
                            property real startX; property real startY; property point origin
                            onPressed: function(mouse) { origin = mapToItem(screenPreview, mouse.x, mouse.y); startX = dialog.geometry.x || 0; startY = dialog.geometry.y || 0 }
                            onPositionChanged: function(mouse) {
                                if (!pressed || !dialog.placement.enabled) return
                                const now = mapToItem(screenPreview, mouse.x, mouse.y)
                                const copy = Object.assign({}, dialog.placement, {anchor: "custom", x: Math.round(startX + (now.x - origin.x) / canvas.zoom), y: Math.round(startY + (now.y - origin.y) / canvas.zoom)})
                                dialog.placement = copy
                            }
                        }
                        Rectangle {
                            objectName: "placementResizeHandle"
                            width: 16; height: 16; anchors.right: parent.right; anchors.bottom: parent.bottom; color: theme.accent
                            MouseArea {
                                anchors.fill: parent; preventStealing: true
                                onPositionChanged: function(mouse) {
                                    if (!pressed || !dialog.placement.enabled) return
                                    const point = mapToItem(screenPreview, mouse.x, mouse.y)
                                    dialog.placement = Object.assign({}, dialog.placement, {unit: "pixels",
                                        width: Math.max(1, Math.round(point.x / canvas.zoom - dialog.geometry.x)),
                                        height: Math.max(1, Math.round(point.y / canvas.zoom - dialog.geometry.y))})
                                }
                            }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ComboBox { model: [qsTr("Percent"), qsTr("Pixels")]; currentIndex: dialog.placement.unit === "pixels" ? 1 : 0; onActivated: { dialog.change("unit", currentIndex === 0 ? "percent" : "pixels"); dialog.change("width", currentIndex === 0 ? 60 : 600); dialog.change("height", currentIndex === 0 ? 60 : 420) } }
                SpinBox { Accessible.name: qsTr("Designer width"); from: 1; to: dialog.placement.unit === "percent" ? 100 : 10000; value: dialog.placement.width || 60; editable: true; onValueModified: dialog.change("width", value) }
                Label { text: "×" }
                SpinBox { Accessible.name: qsTr("Designer height"); from: 1; to: dialog.placement.unit === "percent" ? 100 : 10000; value: dialog.placement.height || 60; editable: true; onValueModified: dialog.change("height", value) }
            }
            RowLayout {
                objectName: "customPositionFields"
                visible: dialog.placement.anchor === "custom"
                Layout.fillWidth: true
                Label { text: qsTr("X") }
                SpinBox {
                    objectName: "placementX"
                    Accessible.name: qsTr("Window X position")
                    Layout.fillWidth: true
                    from: dialog.geometry.usableX || 0
                    to: Math.max(from, (dialog.geometry.usableX || 0)
                        + (dialog.geometry.usableWidth || dialog.geometry.monitorWidth || 1920)
                        - (dialog.geometry.width || 600))
                    value: dialog.placement.x === undefined ? (dialog.geometry.x || 0) : dialog.placement.x
                    editable: true
                    onValueModified: dialog.change("x", value)
                }
                Label { text: qsTr("Y") }
                SpinBox {
                    objectName: "placementY"
                    Accessible.name: qsTr("Window Y position")
                    Layout.fillWidth: true
                    from: dialog.geometry.usableY || 0
                    to: Math.max(from, (dialog.geometry.usableY || 0)
                        + (dialog.geometry.usableHeight || dialog.geometry.monitorHeight || 1080)
                        - (dialog.geometry.height || 420))
                    value: dialog.placement.y === undefined ? (dialog.geometry.y || 0) : dialog.placement.y
                    editable: true
                    onValueModified: dialog.change("y", value)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Anchor") }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["center", "top", "bottom", "left", "right", "top-left", "top-right", "bottom-left", "bottom-right", "custom"]
                    currentIndex: Math.max(0, model.indexOf(dialog.placement.anchor || "center"))
                    onActivated: {
                        if (currentText === "custom") {
                            dialog.change("x", dialog.geometry.x || 0)
                            dialog.change("y", dialog.geometry.y || 0)
                        }
                        dialog.change("anchor", currentText)
                    }
                }
                Label { text: qsTr("Margin") }
                SpinBox { objectName: "placementMargin"; from: 0; to: 1000; value: dialog.placement.margin || 0; editable: true; onValueModified: dialog.change("margin", value) }
            }
            RowLayout {
                CheckBox { text: qsTr("Lock aspect ratio"); checked: !!dialog.placement.aspectLock; onToggled: dialog.change("aspectLock", checked) }
                ComboBox { model: ["3:2", "16:9", "4:3", "1:1"]; currentIndex: Math.max(0, [1.5, 16/9, 4/3, 1].findIndex(r => Math.abs(r - (dialog.placement.aspect || 1.5)) < 0.001)); onActivated: dialog.change("aspect", [1.5, 16/9, 4/3, 1][currentIndex]) }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: dialog.geometry.error || (dialog.placement.enabled ? qsTr("%1 × %2 at %3,%4 on %5%6").arg(dialog.geometry.width || 0).arg(dialog.geometry.height || 0).arg(dialog.geometry.x || 0).arg(dialog.geometry.y || 0).arg(dialog.geometry.monitor || "").arg(dialog.geometry.usedFallback ? qsTr(" · fallback") : "") : qsTr("Designer is off; existing Layout controls remain active.")) }
        }
    }
}
