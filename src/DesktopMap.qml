import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: map
    objectName: "desktopMap"
    required property var host
    property var monitors: backend.monitorPlan
    property var workspaces: backend.workspacePlan
    property var draftPositions: []
    property bool overlaps: false
    property string feedback: ""
    property var draggedWorkspace: null
    property point dragPoint
    property string hoveredDisplay: ""
    radius: 9
    color: host.surface
    border.color: host.outline
    implicitHeight: contentColumn.implicitHeight + 28

    function positionFor(description) {
        for (let i = 0; i < draftPositions.length; ++i)
            if (draftPositions[i].description === description) return draftPositions[i]
        for (let i = 0; i < monitors.length; ++i)
            if (monitors[i].description === description) return monitors[i]
        return ({x: 0, y: 0, width: 0, height: 0})
    }
    function resetPreview() {
        const next = []
        for (let i = 0; i < monitors.length; ++i) {
            const output = monitors[i]
            next.push({description: output.description, x: Number(output.x) || 0,
                y: Number(output.y) || 0, baseX: Number(output.x) || 0,
                baseY: Number(output.y) || 0,
                width: Math.max(1, Number(output.width) || 1),
                height: Math.max(1, Number(output.height) || 1)})
        }
        draftPositions = next
        updateOverlap()
    }
    function setPreviewPosition(description, x, y) {
        const next = draftPositions.map(output => output.description === description
            ? Object.assign({}, output, {x: Math.round(x), y: Math.round(y)}) : output)
        draftPositions = next
        updateOverlap()
    }
    function updateOverlap() {
        let found = false
        for (let i = 0; i < draftPositions.length; ++i) {
            const a = draftPositions[i]
            for (let j = i + 1; j < draftPositions.length; ++j) {
                const b = draftPositions[j]
                if (a.x < b.x + b.width && a.x + a.width > b.x &&
                    a.y < b.y + b.height && a.y + a.height > b.y)
                    found = true
            }
        }
        overlaps = found
    }
    function hasChanges() {
        return draftPositions.some(output => output.x !== output.baseX || output.y !== output.baseY)
    }
    function applyPreview() {
        if (overlaps) {
            feedback = qsTr("Displays overlap in the preview. Move them apart before applying.")
            return
        }
        const changes = draftPositions.filter(output =>
            output.x !== output.baseX || output.y !== output.baseY)
        if (changes.length === 0) return
        for (let i = 0; i < changes.length; ++i) {
            const output = changes[i]
            if (!backend.applyMonitorLayout(output.description, {x: output.x, y: output.y})) {
                feedback = backend.status
                return
            }
        }
        feedback = backend.status
        resetPreview()
    }
    function cancelPreview() {
        resetPreview()
        feedback = qsTr("Move preview discarded. The display layout was not changed.")
    }
    function displayAt(point) {
        const inViewport = mapViewport.mapFromItem(map, point.x, point.y)
        if (!mapViewport.contains(inViewport)) return ""
        for (let i = monitorTiles.count - 1; i >= 0; --i) {
            const tile = monitorTiles.itemAt(i)
            if (!tile) continue
            const description = tile.modelData.description || ""
            if (description && monitors.filter(output => output.description === description).length === 1
                    && tile.contains(tile.mapFromItem(map, point.x, point.y)))
                return description
        }
        return ""
    }

    function finishWorkspaceDrag(point) {
        const workspace = draggedWorkspace
        const target = displayAt(point)
        draggedWorkspace = null
        hoveredDisplay = ""
        if (workspace && target && workspace.monitor !== target)
            host.openWorkspacePin(workspace.slot, workspace.monitor, workspace.persistent, target)
    }

    Component.onCompleted: resetPreview()
    onMonitorsChanged: resetPreview()

    readonly property real minX: {
        if (draftPositions.length === 0) return 0
        let value = draftPositions[0].x
        for (let i = 1; i < draftPositions.length; ++i) value = Math.min(value, draftPositions[i].x)
        return value
    }
    readonly property real minY: {
        if (draftPositions.length === 0) return 0
        let value = draftPositions[0].y
        for (let i = 1; i < draftPositions.length; ++i) value = Math.min(value, draftPositions[i].y)
        return value
    }
    readonly property real boundsWidth: {
        if (draftPositions.length === 0) return 1
        let value = 1
        for (let i = 0; i < draftPositions.length; ++i) {
            const output = draftPositions[i]
            value = Math.max(value, output.x + output.width - minX)
        }
        return value
    }
    readonly property real boundsHeight: {
        if (draftPositions.length === 0) return 1
        let value = 1
        for (let i = 0; i < draftPositions.length; ++i) {
            const output = draftPositions[i]
            value = Math.max(value, output.y + output.height - minY)
        }
        return value
    }
    readonly property real mapScale: Math.max(0.01,
        Math.min((Math.max(map.width - 28, 360) - 52) / boundsWidth, 280 / boundsHeight))
    readonly property real mapContentWidth: Math.max(map.width - 28, boundsWidth * mapScale + 48)
    readonly property real mapContentHeight: Math.max(220, boundsHeight * mapScale + 48)

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label { text: qsTr("Display map"); font.pixelSize: 17; font.bold: true }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Drag a workspace card onto a display to preview its pin, or select the card to choose a display. Use the arrows to preview display positions.")
                    color: host.mutedForeground
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                }
            }
            Label {
                objectName: "desktopMapLayoutState"
                text: backend.monitorRevertPending ? qsTr("TRIAL") :
                    (map.hasChanges() ? qsTr("PREVIEW") : qsTr("LIVE"))
                color: backend.monitorRevertPending ? "#ffbd72" :
                    (map.hasChanges() ? theme.accent : "#8bd5a5")
                font.bold: true
                font.pixelSize: 10
            }
        }

        Flickable {
            id: mapViewport
            objectName: "desktopMapViewport"
            Layout.fillWidth: true
            Layout.preferredHeight: map.mapContentHeight
            contentWidth: map.mapContentWidth
            contentHeight: map.mapContentHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.horizontal: ScrollBar { policy: map.mapContentWidth > mapViewport.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
            ScrollBar.vertical: ScrollBar { policy: map.mapContentHeight > mapViewport.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

            Item {
                id: canvas
                width: map.mapContentWidth
                height: map.mapContentHeight

                Repeater {
                    id: monitorTiles
                    model: map.monitors
                    delegate: Rectangle {
                        id: monitorTile
                        required property var modelData
                        required property int index
                        readonly property var draft: map.positionFor(modelData.description)
                        readonly property var openWindows: backend.windows.filter(window =>
                            window.monitorName === modelData.name)
                        objectName: "desktopMapMonitor" + index
                        x: 24 + (draft.x - map.minX) * map.mapScale
                        y: 24 + (draft.y - map.minY) * map.mapScale
                        width: Math.max(150, draft.width * map.mapScale)
                        height: Math.max(112, draft.height * map.mapScale)
                        radius: 8
                        color: modelData.focused ? Qt.alpha(theme.accent, 0.13) : "#101a21"
                        border.width: 2
                        border.color: (map.hoveredDisplay !== "" && map.hoveredDisplay === modelData.description)
                            || modelData.focused ? theme.accent :
                            (draft.x !== draft.baseX || draft.y !== draft.baseY ? "#ffbd72" : host.outline)

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 9
                            spacing: 4
                            Rectangle {
                                objectName: "desktopMapDragHandle" + monitorTile.index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                radius: 5
                                color: Qt.alpha(theme.accent, 0.17)
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    Label { Layout.fillWidth: true; text: monitorTile.modelData.name || monitorTile.modelData.description; font.bold: true; elide: Text.ElideRight }
                                    Label { text: qsTr("POSITION"); color: theme.accent; font.pixelSize: 9; font.bold: true }
                                }
                            }
                            Label {
                                objectName: "desktopMapActiveWorkspace" + monitorTile.index
                                Layout.fillWidth: true
                                text: monitorTile.modelData.description || qsTr("Display identity unavailable")
                                color: host.mutedForeground
                                font.pixelSize: 10
                                elide: Text.ElideMiddle
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("%1×%2  ·  x %3, y %4")
                                    .arg(monitorTile.modelData.width || 0)
                                    .arg(monitorTile.modelData.height || 0)
                                    .arg(monitorTile.draft.x).arg(monitorTile.draft.y)
                                font.pixelSize: 10
                                color: monitorTile.draft.x !== monitorTile.draft.baseX ||
                                    monitorTile.draft.y !== monitorTile.draft.baseY ? "#ffbd72" : "#c2cdd6"
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Active workspace %1").arg(
                                    monitorTile.modelData.activeWorkspace
                                        ? monitorTile.modelData.activeWorkspace.name : "—")
                                color: theme.accent
                                font.pixelSize: 10
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: {
                                    const slots = map.workspaces.filter(workspace => workspace.monitor
                                        && workspace.monitor === monitorTile.modelData.description)
                                        .map(workspace => workspace.slot)
                                    return slots.length ? qsTr("Pinned workspaces: %1").arg(slots.join(", ")) : ""
                                }
                                elide: Text.ElideRight
                                font.pixelSize: 10
                            }
                            Label {
                                objectName: "desktopMapOpenWindows" + monitorTile.index
                                Layout.fillWidth: true
                                text: monitorTile.openWindows.length > 0
                                    ? qsTr("Open: %1").arg(monitorTile.openWindows
                                        .map(window => window.currentTitle || window["class"]).join(", "))
                                    : qsTr("No open windows")
                                color: monitorTile.openWindows.length > 0 ? "#d5dde2" : host.mutedForeground
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                ToolButton {
                                    objectName: "desktopMapMoveLeft" + monitorTile.index
                                    text: "←"
                                    Accessible.name: qsTr("Move %1 left by 100 logical pixels").arg(monitorTile.modelData.name)
                                    onClicked: map.setPreviewPosition(monitorTile.modelData.description,
                                        monitorTile.draft.x - 100, monitorTile.draft.y)
                                }
                                ToolButton {
                                    objectName: "desktopMapMoveRight" + monitorTile.index
                                    text: "→"
                                    Accessible.name: qsTr("Move %1 right by 100 logical pixels").arg(monitorTile.modelData.name)
                                    onClicked: map.setPreviewPosition(monitorTile.modelData.description,
                                        monitorTile.draft.x + 100, monitorTile.draft.y)
                                }
                                ToolButton {
                                    objectName: "desktopMapMoveUp" + monitorTile.index
                                    text: "↑"
                                    Accessible.name: qsTr("Move %1 up by 100 logical pixels").arg(monitorTile.modelData.name)
                                    onClicked: map.setPreviewPosition(monitorTile.modelData.description,
                                        monitorTile.draft.x, monitorTile.draft.y - 100)
                                }
                                ToolButton {
                                    objectName: "desktopMapMoveDown" + monitorTile.index
                                    text: "↓"
                                    Accessible.name: qsTr("Move %1 down by 100 logical pixels").arg(monitorTile.modelData.name)
                                    onClicked: map.setPreviewPosition(monitorTile.modelData.description,
                                        monitorTile.draft.x, monitorTile.draft.y + 100)
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: map.monitors.length === 0
                    text: qsTr("No connected display data. Refresh to try again.")
                    color: host.mutedForeground
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: map.workspaces
                delegate: Button {
                    id: workspaceCard
                    required property var modelData
                    objectName: "desktopMapWorkspace" + modelData.slot
                    text: qsTr("Workspace %1 · %2%3").arg(modelData.slot)
                        .arg(modelData.monitor ? modelData.monitor : qsTr("Unpinned"))
                        .arg((modelData.apps || []).length > 0
                            ? qsTr(" · %1 app(s)").arg(modelData.apps.length) : "")
                    Accessible.name: qsTr("Plan workspace %1 display; currently %2")
                        .arg(modelData.slot).arg(modelData.monitor || qsTr("unpinned"))
                    onClicked: map.host.openWorkspacePin(modelData.slot, modelData.monitor,
                        modelData.persistent, "")
                    MouseArea {
                        id: workspaceDrag
                        anchors.fill: parent
                        preventStealing: true
                        cursorShape: map.draggedWorkspace ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                        property point start
                        onPressed: mouse => {
                            workspaceCard.forceActiveFocus(Qt.MouseFocusReason)
                            start = map.mapFromItem(workspaceDrag, mouse.x, mouse.y)
                        }
                        onPositionChanged: mouse => {
                            if (!pressed || backend.monitorRevertPending || backend.trialActive) return
                            const point = map.mapFromItem(workspaceDrag, mouse.x, mouse.y)
                            if (!map.draggedWorkspace
                                    && Math.hypot(point.x - start.x, point.y - start.y) < 8)
                                return
                            map.draggedWorkspace = workspaceCard.modelData
                            map.dragPoint = point
                            map.hoveredDisplay = map.displayAt(point)
                        }
                        onReleased: mouse => {
                            if (map.draggedWorkspace)
                                map.finishWorkspaceDrag(map.mapFromItem(workspaceDrag, mouse.x, mouse.y))
                            else if (containsMouse)
                                workspaceCard.clicked()
                        }
                        onCanceled: {
                            map.draggedWorkspace = null
                            map.hoveredDisplay = ""
                        }
                    }
                }
            }
        }

        Label {
            objectName: "desktopMapOverlapWarning"
            Layout.fillWidth: true
            visible: map.overlaps
            color: "#ffbd72"
            wrapMode: Text.WordWrap
            text: qsTr("Two displays overlap in this preview. Move them apart before applying.")
        }
        Label {
            Layout.fillWidth: true
            visible: backend.monitorRevertPending
            color: "#ffbd72"
            wrapMode: Text.WordWrap
            text: qsTr("Layout trial is active. Keep it to save, or revert it now. It will automatically revert after 15 seconds.")
        }
        RowLayout {
            Layout.fillWidth: true
            Label { Layout.fillWidth: true; text: map.feedback; color: host.mutedForeground; wrapMode: Text.WordWrap }
            Button {
                objectName: "desktopMapCancelAction"
                visible: !backend.monitorRevertPending
                text: qsTr("Cancel preview")
                enabled: map.hasChanges()
                onClicked: map.cancelPreview()
            }
            Button {
                objectName: "desktopMapApplyAction"
                visible: !backend.monitorRevertPending
                text: qsTr("Apply preview…")
                enabled: map.hasChanges() && !map.overlaps && !backend.trialActive
                onClicked: map.applyPreview()
                background: Rectangle { radius: 7; color: parent.enabled ? theme.accent : "#303941" }
                contentItem: Label { text: qsTr("Apply preview…"); horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: parent.enabled ? "#111820" : "#87939a" }
            }
            Button {
                objectName: "desktopMapKeepAction"
                visible: backend.monitorRevertPending
                text: qsTr("Keep layout")
                onClicked: { backend.keepMonitorLayout(); map.feedback = backend.status }
            }
            Button {
                objectName: "desktopMapRevertAction"
                visible: backend.monitorRevertPending
                text: qsTr("Revert layout")
                onClicked: { backend.revertMonitorLayout(); map.feedback = backend.status; map.resetPreview() }
            }
        }
    }
    Label {
        visible: map.draggedWorkspace !== null
        z: 2
        x: map.dragPoint.x + 12
        y: map.dragPoint.y + 12
        padding: 8
        text: map.draggedWorkspace ? qsTr("Workspace %1").arg(map.draggedWorkspace.slot) : ""
        color: "#111820"
        background: Rectangle { color: theme.accent; radius: 5 }
    }
}
