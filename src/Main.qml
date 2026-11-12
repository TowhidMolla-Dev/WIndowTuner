import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1120
    height: 820
    minimumWidth: 860
    minimumHeight: 680
    visible: true
    title: qsTr("WindowTuner")
    color: "#0e1110"

    // Keep WindowTuner visually aligned with Omarchy/Hyprforge surfaces while
    // retaining its own focused window-rule workflow.
    readonly property color surface: "#151a18"
    readonly property color raisedSurface: "#1c2320"
    readonly property color outline: "#29332e"
    readonly property color mutedForeground: "#98a59e"
    readonly property var sections: [
        { title: qsTr("Layout"), icon: "▦", subtitle: qsTr("Shape and size this window") },
        { title: qsTr("Placement"), icon: "⌖", subtitle: qsTr("Choose where and how it appears") },
        { title: qsTr("Appearance"), icon: "◐", subtitle: qsTr("Tune transparency and surfaces") },
        { title: qsTr("Behavior"), icon: "⌘", subtitle: qsTr("Set focus and performance behavior") },
        { title: qsTr("Rules"), icon: "☷", subtitle: qsTr("Manage saved window rules") }
    ]

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: Material.Small

    property string layoutMode: "floating"
    property int category: 0
    property int editingRuleIndex: -1
    property bool previewExpanded: false
    property var visibleWindows: {
        const needle = windowSearch.text.trim().toLocaleLowerCase()
        const rows = backend.windows
        const result = []
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            const searchable = (row.currentTitle + " " + row.title + " " + row["class"] + " " + row.workspace).toLocaleLowerCase()
            if (!needle || searchable.indexOf(needle) !== -1) {
                const item = Object.assign({}, row)
                item.sourceIndex = i
                result.push(item)
            }
        }
        return result
    }
    property var visibleRules: {
        const needle = savedRuleSearch.text.trim().toLocaleLowerCase()
        const result = []
        const rows = backend.savedRules
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            const text = ((row.name || "") + " " + (row["class"] || "") + " " + (row.title || "")).toLocaleLowerCase()
            if (!needle || text.indexOf(needle) !== -1) {
                const item = Object.assign({}, row)
                item.sourceIndex = i
                result.push(item)
            }
        }
        return result
    }

    function options() {
        const target = backend.selectedWindow
        const match = {}
        if (target["class"]) match["class"] = target["class"]
        if (titleMatch.checked && target.title) match.title = target.title
        if (workspaceMatch.checked && target.workspace) match.workspace = target.workspace
        if (xdgTagMatch.checked && target.xdgTag) match.xdg_tag = target.xdgTag
        if (xwaylandMatch.checked) match.xwayland = target.xwayland
        if (fullscreenMatch.checked) match.fullscreen = target.fullscreen !== 0
        if (groupMatch.checked) match.group = target.grouped
        if (contentMatch.checked) match.content = target.contentType
        return {
            layout: layoutMode,
            center: centerBox.checked,
            width: widthField.value,
            height: heightField.value,
            setPosition: positionBox.checked,
            x: xField.value,
            y: yField.value,
            matchTitle: titleMatch.checked,
            classMatchMode: ["exact", "contains", "regex"][classMatchMode.currentIndex],
            titleMatchMode: ["exact", "contains", "regex"][titleMatchMode.currentIndex],
            excludeTitle: excludeTitleField.text,
            match: match,
            pin: pinBox.checked,
            noBlur: blurBox.checked,
            noShadow: shadowBox.checked,
            noInitialFocus: focusBox.checked,
            noAnim: animationBox.checked,
            noDim: dimBox.checked,
            noVrr: vrrBox.checked,
            persistentSize: persistentSizeBox.checked,
            roundingEnabled: roundingBox.checked,
            rounding: roundingField.value,
            minSizeEnabled: minSizeBox.checked,
            minWidth: minWidthField.value,
            minHeight: minHeightField.value,
            maxSizeEnabled: maxSizeBox.checked,
            maxWidth: maxWidthField.value,
            maxHeight: maxHeightField.value,
            opacityEnabled: opacityBox.checked,
            opacityActive: activeOpacity.value,
            opacityInactive: inactiveOpacity.value,
            borderEnabled: borderBox.checked,
            borderSize: borderSize.value,
            workspace: workspaceField.text
        }
    }

    function refreshPreview() { preview.text = backend.previewRule(options()) }
    function loadRule(index) {
        const saved = backend.loadRuleForEditing(index)
        if (!saved || Object.keys(saved).length === 0) return
        category = 0
        const match = saved.match || {}
        editingRuleIndex = index
        layoutMode = saved.layout || "floating"
        widthField.value = saved.width === undefined ? 600 : saved.width
        heightField.value = saved.height === undefined ? 420 : saved.height
        centerBox.checked = !!saved.center
        positionBox.checked = !!saved.setPosition
        xField.value = saved.x || 0
        yField.value = saved.y || 0
        titleMatch.checked = match.title !== undefined
        titleMatchMode.currentIndex = ["exact", "contains", "regex"].indexOf(saved.titleMatchMode || "exact")
        classMatchMode.currentIndex = ["exact", "contains", "regex"].indexOf(saved.classMatchMode || "exact")
        workspaceMatch.checked = match.workspace !== undefined
        xdgTagMatch.checked = match.xdg_tag !== undefined
        xwaylandMatch.checked = match.xwayland !== undefined
        fullscreenMatch.checked = match.fullscreen !== undefined
        groupMatch.checked = match.group !== undefined
        contentMatch.checked = match.content !== undefined
        excludeTitleField.text = saved.excludeTitle || ""
        pinBox.checked = !!saved.pin
        blurBox.checked = !!saved.noBlur
        shadowBox.checked = !!saved.noShadow
        focusBox.checked = !!saved.noInitialFocus
        animationBox.checked = !!saved.noAnim
        dimBox.checked = !!saved.noDim
        vrrBox.checked = !!saved.noVrr
        persistentSizeBox.checked = !!saved.persistentSize
        roundingBox.checked = !!saved.roundingEnabled
        roundingField.value = saved.rounding === undefined ? 8 : saved.rounding
        minSizeBox.checked = !!saved.minSizeEnabled
        minWidthField.value = saved.minWidth || 320
        minHeightField.value = saved.minHeight || 200
        maxSizeBox.checked = !!saved.maxSizeEnabled
        maxWidthField.value = saved.maxWidth || 1200
        maxHeightField.value = saved.maxHeight || 900
        opacityBox.checked = !!saved.opacityEnabled
        activeOpacity.value = saved.opacityActive === undefined ? 1 : saved.opacityActive
        inactiveOpacity.value = saved.opacityInactive === undefined ? 0.9 : saved.opacityInactive
        borderBox.checked = !!saved.borderEnabled
        borderSize.value = saved.borderSize || 2
        workspaceField.text = saved.workspace || ""
        refreshPreview()
    }
    function applyPreset(width, height, centered, x, y) {
        layoutMode = "floating"
        widthField.value = width
        heightField.value = height
        centerBox.checked = centered
        positionBox.checked = !centered
        xField.value = x
        yField.value = y
        refreshPreview()
    }

    Dialog {
        id: deleteRuleDialog
        property int ruleIndex: -1
        anchors.centerIn: parent
        modal: true
        title: qsTr("Remove saved rule?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        Label {
            width: 340
            text: qsTr("This removes the persistent rule from your Hyprland config. Matching windows will use default behavior after reload.")
            wrapMode: Text.WordWrap
        }
        onAccepted: backend.deleteRule(ruleIndex)
    }

    Dialog {
        id: rulesDataDialog
        property bool importing: false
        anchors.centerIn: parent
        modal: true
        width: Math.min(win.width - 60, 680)
        title: importing ? qsTr("Import rules") : qsTr("Export rules")
        standardButtons: importing ? Dialog.Cancel | Dialog.Ok : Dialog.Close
        contentItem: TextArea {
            id: rulesJson
            implicitHeight: 360
            selectByMouse: true
            wrapMode: TextEdit.NoWrap
            font.family: "monospace"
            text: ""
        }
        onOpened: {
            if (!importing) rulesJson.text = backend.exportRules()
            else rulesJson.text = ""
        }
        onAccepted: if (importing) backend.importRules(rulesJson.text)
    }

    Window {
        id: pickerOverlay
        visible: false
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        color: "#d9101114"
        title: qsTr("Pick a window")
        x: Screen.virtualX
        y: Screen.virtualY
        width: Screen.width
        height: Screen.height
        Shortcut { sequence: "Escape"; context: Qt.WindowShortcut; onActivated: pickerOverlay.visible = false }
        Rectangle {
            anchors.centerIn: parent
            width: pickHint.implicitWidth + 52
            height: pickHint.implicitHeight + 34
            radius: 13
        color: win.raisedSurface
            border.color: theme.accent
            Text {
                id: pickHint
                anchors.centerIn: parent
                text: qsTr("Click a window to select it  ·  Esc to cancel")
                color: "#f4f4f5"
                font.pixelSize: 17
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.CrossCursor
            onClicked: function(mouse) {
                backend.pickWindowAt(Math.round(pickerOverlay.x + mouse.x), Math.round(pickerOverlay.y + mouse.y))
                pickerOverlay.visible = false
            }
        }
    }

    Connections {
        target: backend
        function onSelectedChanged() { win.refreshPreview() }
        function onWindowsChanged() { win.refreshPreview() }
    }

    Shortcut { sequence: "Ctrl+R"; context: Qt.ApplicationShortcut; onActivated: backend.refreshWindows() }
    Shortcut {
        sequence: "Ctrl+S"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.editingRuleIndex >= 0) {
                if (backend.updateRule(win.editingRuleIndex, win.options())) win.editingRuleIndex = -1
            } else if (backend.selectedIndex >= 0) backend.saveRule(win.options())
        }
    }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: { win.category === 4 ? savedRuleSearch.forceActiveFocus() : windowSearch.forceActiveFocus(); (win.category === 4 ? savedRuleSearch : windowSearch).selectAll() } }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: win.editingRuleIndex >= 0; onActivated: win.editingRuleIndex = -1 }
    Shortcut { sequence: "Alt+1"; context: Qt.ApplicationShortcut; onActivated: win.category = 0 }
    Shortcut { sequence: "Alt+2"; context: Qt.ApplicationShortcut; onActivated: win.category = 1 }
    Shortcut { sequence: "Alt+3"; context: Qt.ApplicationShortcut; onActivated: win.category = 2 }
    Shortcut { sequence: "Alt+4"; context: Qt.ApplicationShortcut; onActivated: win.category = 3 }
    Shortcut { sequence: "Alt+5"; context: Qt.ApplicationShortcut; onActivated: win.category = 4 }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // A restrained app bar separates the product identity from the task.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            color: "#111613"
            border.color: win.outline
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 20
                spacing: 13

                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 11
                    color: Qt.alpha(theme.accent, 0.14)
                    border.color: Qt.alpha(theme.accent, 0.35)
                    Text { anchors.centerIn: parent; text: "✦"; color: theme.accent; font.bold: true; font.pixelSize: 19 }
                }
                ColumnLayout {
                    spacing: 1
                    Label { text: qsTr("WindowTuner"); font.pixelSize: 18; font.bold: true }
                    Label { text: qsTr("WINDOW RULE STUDIO"); font.pixelSize: 9; font.bold: true; color: win.mutedForeground; opacity: 0.78 }
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("%1 WINDOWS").arg(backend.windows.length)
                    color: win.mutedForeground
                    font.pixelSize: 9
                    font.bold: true
                    leftPadding: 10
                    rightPadding: 10
                    background: Rectangle { radius: 7; color: win.raisedSurface; border.color: win.outline }
                    Layout.rightMargin: 6
                }
                ToolButton {
                    text: "↻"
                    font.pixelSize: 20
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Refresh window list · Ctrl+R")
                    onClicked: backend.refreshWindows()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 18
            spacing: 16

            // Window selection is intentionally always visible while editing.
            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                radius: 14
                color: win.surface
                border.color: win.outline

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("WINDOWS"); font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.1 }
                        Item { Layout.fillWidth: true }
                        ToolButton {
                            text: "⌖"
                            font.pixelSize: 19
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Pick a window on screen")
                            onClicked: pickerOverlay.visible = true
                        }
                        Label {
                            text: qsTr("%1 LIVE").arg(backend.windows.length)
                            color: win.mutedForeground
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                    TextField {
                        id: windowSearch
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search title or app…")
                        selectByMouse: true
                    }

                    ListView {
                        id: windowList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 5
                        model: win.visibleWindows
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: ItemDelegate {
                            id: windowItem
                            required property var modelData
                            width: ListView.view.width
                            height: 66
                            highlighted: modelData.sourceIndex === backend.selectedIndex
                            onClicked: backend.selectWindow(modelData.sourceIndex)
                            background: Rectangle {
                                radius: 9
                                    color: windowItem.highlighted ? Qt.alpha(theme.accent, 0.11) : (windowItem.hovered ? win.raisedSurface : "transparent")
                                    border.width: windowItem.highlighted ? 1 : 0
                                    border.color: Qt.alpha(theme.accent, 0.48)
                            }
                            contentItem: RowLayout {
                                spacing: 11
                                Rectangle {
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34
                                    radius: 9
                                    color: windowItem.highlighted ? Qt.alpha(theme.accent, 0.19) : "#25272c"
                                    Text {
                                        anchors.centerIn: parent
                                        text: (modelData["class"] || "?").charAt(0).toUpperCase()
                                        color: windowItem.highlighted ? theme.accent : "#c1c3ca"
                                        font.bold: true
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Label {
                                        Layout.fillWidth: true
                                text: modelData.currentTitle || modelData.title || qsTr("Untitled window")
                                        elide: Text.ElideRight
                                        font.pixelSize: 13
                                        font.bold: windowItem.highlighted
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                text: modelData["class"] + "  ·  WS " + modelData.workspace + "  ·  " + (modelData.monitorName || qsTr("monitor %1").arg(modelData.monitor))
                                        elide: Text.ElideRight
                                        opacity: 0.58
                                        font.pixelSize: 10
                                    }
                                }
                                Rectangle {
                                    visible: modelData.floating
                                    Layout.preferredWidth: 7
                                    Layout.preferredHeight: 7
                                    radius: 4
                                    color: "#79d49b"
                                    ToolTip.visible: floatingHint.hovered
                                    ToolTip.text: qsTr("Currently floating")
                                    HoverHandler { id: floatingHint }
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: win.visibleWindows.length === 0
                            text: backend.windows.length === 0 ? qsTr("No windows found") : qsTr("No matching windows")
                            opacity: 0.55
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: selectedInfo.implicitHeight + 22
                        radius: 10
                        color: win.raisedSurface
                        visible: backend.selectedIndex >= 0
                        ColumnLayout {
                            id: selectedInfo
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 11
                            spacing: 4
                            Label { text: qsTr("RULE TARGET"); font.pixelSize: 9; font.bold: true; color: theme.accent }
                            Label {
                                Layout.fillWidth: true
                                text: backend.selectedIndex >= 0 ? backend.windows[backend.selectedIndex].title || qsTr("No initial title") : ""
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: backend.selectedIndex >= 0 ? backend.windows[backend.selectedIndex]["class"] : ""
                                font.family: "monospace"
                                font.pixelSize: 10
                                opacity: 0.58
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: backend.selectedIndex >= 0 ? qsTr("PID %1 · %2").arg(backend.windows[backend.selectedIndex].pid).arg(backend.windows[backend.selectedIndex].address) : ""
                                font.family: "monospace"
                                font.pixelSize: 9
                                opacity: 0.48
                                elide: Text.ElideMiddle
                            }
                            Label {
                                Layout.fillWidth: true
                                text: backend.selectedIndex >= 0 ? qsTr("%1 × %2 · %3 · %4").arg(backend.windows[backend.selectedIndex].size[0]).arg(backend.windows[backend.selectedIndex].size[1]).arg(backend.windows[backend.selectedIndex].fullscreen ? qsTr("fullscreen") : backend.windows[backend.selectedIndex].floating ? qsTr("floating") : qsTr("tiled")).arg(backend.windows[backend.selectedIndex].xwayland ? "XWayland" : "Wayland") : ""
                                font.pixelSize: 9
                                opacity: 0.48
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 2
                        Label { text: qsTr("WINDOWTUNER  /  %1 OF 5").arg(win.category + 1); font.pixelSize: 9; font.bold: true; color: theme.accent; font.letterSpacing: 0.9 }
                        Label { text: win.sections[win.category].title; font.pixelSize: 23; font.bold: true }
                        Label { text: win.sections[win.category].subtitle; color: win.mutedForeground; font.pixelSize: 11 }
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: (win.previewExpanded ? "⌄  " + qsTr("Hide preview") : "◉  " + qsTr("Preview rule"))
                        flat: true
                        highlighted: win.previewExpanded
                        onClicked: win.previewExpanded = !win.previewExpanded
                    }
                }

                // Segmented section navigation, following Hyprforge's compact
                // studio navigation while keeping WindowTuner's own workflows.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    Repeater {
                        model: win.sections
                        delegate: Button {
                            id: sectionButton
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            checkable: true
                            checked: win.category === index
                            flat: true
                            onClicked: win.category = index
                            background: Rectangle {
                                radius: 9
                                color: sectionButton.checked ? Qt.alpha(theme.accent, 0.13) : sectionButton.hovered ? win.raisedSurface : "transparent"
                                border.width: sectionButton.checked ? 1 : 0
                                border.color: Qt.alpha(theme.accent, 0.38)
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            contentItem: RowLayout {
                                spacing: 7
                                Text {
                                    text: modelData.icon
                                    color: sectionButton.checked ? theme.accent : win.mutedForeground
                                    font.pixelSize: 14
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    horizontalAlignment: Text.AlignHCenter
                                    color: sectionButton.checked ? theme.accent : "#c2cbc5"
                                    font.pixelSize: 10
                                    font.bold: sectionButton.checked
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    id: settingsScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    StackLayout {
                        width: settingsScroll.availableWidth
                        currentIndex: win.category

                        // LAYOUT
                        Item {
                            implicitHeight: layoutContent.implicitHeight
                            ColumnLayout {
                                id: layoutContent
                                width: parent.width
                                spacing: 12

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: layoutCard.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: layoutCard
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 14
                                        ColumnLayout {
                                            spacing: 3
                                            Label { text: qsTr("Window layout"); font.pixelSize: 15; font.bold: true }
                                            Label { text: qsTr("Control how this window joins your workspace."); font.pixelSize: 11; opacity: 0.58 }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8
                                            Repeater {
                                                model: [qsTr("Floating"), qsTr("Keep current"), qsTr("Tiled")]
                                                delegate: Button {
                                                    required property string modelData
                                                    required property int index
                                                    Layout.fillWidth: true
                                                    text: modelData
                                                    checkable: true
                                                    checked: ["floating", "keep", "tiled"][index] === win.layoutMode
                                                    onClicked: { win.layoutMode = ["floating", "keep", "tiled"][index]; win.refreshPreview() }
                                                }
                                            }
                                        }
                                        Label { text: qsTr("Quick presets"); font.pixelSize: 11; opacity: 0.65 }
                                        Flow {
                                            Layout.fillWidth: true
                                            spacing: 7
                                            Button { text: qsTr("Compact · 500 × 500"); onClicked: win.applyPreset(500, 500, true, 0, 0) }
                                            Button { text: qsTr("Picture-in-picture"); onClicked: win.applyPreset(420, 280, false, 24, 64) }
                                            Button { text: qsTr("Large · 900 × 700"); onClicked: win.applyPreset(900, 700, true, 0, 0) }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: sizeContent.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: sizeContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 12
                                        ColumnLayout {
                                            spacing: 3
                                            Label { text: qsTr("Size & position"); font.pixelSize: 15; font.bold: true }
                                            Label { text: qsTr("Set dimensions in logical pixels. Position is relative to the monitor."); font.pixelSize: 11; opacity: 0.58; wrapMode: Text.WordWrap }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 12
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Label { text: qsTr("Width"); font.pixelSize: 11; opacity: 0.7 }
                                                SpinBox { id: widthField; Layout.fillWidth: true; from: 100; to: 10000; value: 600; editable: true; onValueChanged: win.refreshPreview() }
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Label { text: qsTr("Height"); font.pixelSize: 11; opacity: 0.7 }
                                                SpinBox { id: heightField; Layout.fillWidth: true; from: 100; to: 10000; value: 420; editable: true; onValueChanged: win.refreshPreview() }
                                            }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            CheckBox { id: centerBox; text: qsTr("Center on monitor"); checked: true; onToggled: { if (checked) positionBox.checked = false; win.refreshPreview() } }
                                            Item { Layout.fillWidth: true }
                                            CheckBox { id: positionBox; text: qsTr("Set position"); onToggled: { if (checked) centerBox.checked = false; win.refreshPreview() } }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            enabled: positionBox.checked
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Label { text: qsTr("X coordinate"); font.pixelSize: 11; opacity: 0.7 }
                                                SpinBox { id: xField; Layout.fillWidth: true; from: 0; to: 10000; value: 80; onValueChanged: win.refreshPreview() }
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Label { text: qsTr("Y coordinate"); font.pixelSize: 11; opacity: 0.7 }
                                                SpinBox { id: yField; Layout.fillWidth: true; from: 0; to: 10000; value: 80; onValueChanged: win.refreshPreview() }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: constraintsContent.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: constraintsContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 8
                                        Label { text: qsTr("Size constraints"); font.pixelSize: 15; font.bold: true }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            CheckBox { id: minSizeBox; text: qsTr("Minimum"); onToggled: win.refreshPreview() }
                                            SpinBox { id: minWidthField; from: 1; to: 10000; value: 320; enabled: minSizeBox.checked; onValueChanged: win.refreshPreview() }
                                            Label { text: "×"; opacity: 0.6 }
                                            SpinBox { id: minHeightField; from: 1; to: 10000; value: 200; enabled: minSizeBox.checked; onValueChanged: win.refreshPreview() }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            CheckBox { id: maxSizeBox; text: qsTr("Maximum"); onToggled: win.refreshPreview() }
                                            SpinBox { id: maxWidthField; from: 1; to: 10000; value: 1200; enabled: maxSizeBox.checked; onValueChanged: win.refreshPreview() }
                                            Label { text: "×"; opacity: 0.6 }
                                            SpinBox { id: maxHeightField; from: 1; to: 10000; value: 900; enabled: maxSizeBox.checked; onValueChanged: win.refreshPreview() }
                                        }
                                    }
                                }
                            }
                        }

                        // PLACEMENT
                        Item {
                            implicitHeight: placementContent.implicitHeight
                            ColumnLayout {
                                id: placementContent
                                width: parent.width
                                spacing: 12
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: placementCard.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: placementCard
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 14
                                        Label { text: qsTr("Workspace & visibility"); font.pixelSize: 15; font.bold: true }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 5
                                            Label { text: qsTr("Workspace"); font.pixelSize: 11; opacity: 0.7 }
                                            TextField {
                                                id: workspaceField
                                                Layout.fillWidth: true
                                                placeholderText: qsTr("Leave empty to keep the current workspace")
                                                onTextChanged: win.refreshPreview()
                                            }
                                            Label { text: qsTr("Examples: 2, 3 silent, name:chat"); font.pixelSize: 10; opacity: 0.5 }
                                        }
                                        Rectangle { Layout.fillWidth: true; height: 1; color: win.outline }
                                        CheckBox { id: pinBox; text: qsTr("Pin on every workspace"); onToggled: win.refreshPreview() }
                                        Label { text: qsTr("Pinned windows stay visible when you switch workspaces."); font.pixelSize: 11; opacity: 0.55; wrapMode: Text.WordWrap }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: scopeCard.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: scopeCard
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 9
                                        Label { text: qsTr("Rule matching"); font.pixelSize: 15; font.bold: true }
                        CheckBox { id: titleMatch; text: qsTr("Match this window's initial title too"); checked: true; onToggled: win.refreshPreview() }
                        RowLayout {
                            Layout.fillWidth: true
                            enabled: titleMatch.checked
                            Label { text: qsTr("Title match"); Layout.preferredWidth: 105; font.pixelSize: 11; opacity: 0.7 }
                            ComboBox { id: titleMatchMode; model: [qsTr("Exact"), qsTr("Contains"), qsTr("Regular expression")]; Layout.fillWidth: true; onCurrentIndexChanged: win.refreshPreview() }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Class match"); Layout.preferredWidth: 105; font.pixelSize: 11; opacity: 0.7 }
                            ComboBox { id: classMatchMode; model: [qsTr("Exact"), qsTr("Contains"), qsTr("Regular expression")]; Layout.fillWidth: true; onCurrentIndexChanged: win.refreshPreview() }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            enabled: !titleMatch.checked
                            Label { text: qsTr("Exclude initial titles (regular expression)"); font.pixelSize: 11; opacity: 0.7 }
                            TextField {
                                id: excludeTitleField
                                Layout.fillWidth: true
                                placeholderText: qsTr("e.g. ^Settings$")
                                onTextChanged: win.refreshPreview()
                            }
                            Label { text: qsTr("Hyprland's negative: selector excludes matching titles."); font.pixelSize: 10; opacity: 0.5 }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: win.outline }
                        Label { text: qsTr("Additional match conditions"); font.pixelSize: 12; font.bold: true }
                        CheckBox { id: workspaceMatch; text: qsTr("Only on the selected window's workspace"); onToggled: win.refreshPreview() }
                        CheckBox { id: xdgTagMatch; text: qsTr("Match the XDG tag"); enabled: backend.selectedWindow.xdgTag !== ""; onToggled: win.refreshPreview() }
                        CheckBox { id: xwaylandMatch; text: qsTr("Match XWayland windows"); onToggled: win.refreshPreview() }
                        CheckBox { id: fullscreenMatch; text: qsTr("Match fullscreen state"); onToggled: win.refreshPreview() }
                        CheckBox { id: groupMatch; text: qsTr("Match grouped state"); onToggled: win.refreshPreview() }
                        CheckBox { id: contentMatch; text: qsTr("Match content type"); enabled: backend.selectedWindow.contentType !== ""; onToggled: win.refreshPreview() }
                                        Label {
                                            Layout.fillWidth: true
                                            text: qsTr("Recommended. The rule matches this app class and title, so other windows from the same app are left alone. Identical class/title windows share the rule.")
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: 11
                                            opacity: 0.62
                                        }
                                    }
                                }
                            }
                        }

                        // APPEARANCE
                        Item {
                            implicitHeight: appearanceContent.implicitHeight
                            ColumnLayout {
                                id: appearanceContent
                                width: parent.width
                                spacing: 12
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: opacityContent.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: opacityContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 10
                                        RowLayout {
                                            Layout.fillWidth: true
                                            ColumnLayout {
                                                spacing: 3
                                                Label { text: qsTr("Opacity"); font.pixelSize: 15; font.bold: true }
                                                Label { text: qsTr("Adjust transparency for focused and unfocused states."); font.pixelSize: 11; opacity: 0.58 }
                                            }
                                            Item { Layout.fillWidth: true }
                                            CheckBox { id: opacityBox; text: qsTr("Enable"); onToggled: win.refreshPreview() }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            enabled: opacityBox.checked
                                            Label { Layout.preferredWidth: 95; text: qsTr("Focused") }
                                            Slider { id: activeOpacity; Layout.fillWidth: true; from: 0.1; to: 1; value: 1; onValueChanged: win.refreshPreview() }
                                            Label { Layout.preferredWidth: 38; text: activeOpacity.value.toFixed(2); font.family: "monospace" }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            enabled: opacityBox.checked
                                            Label { Layout.preferredWidth: 95; text: qsTr("Unfocused") }
                                            Slider { id: inactiveOpacity; Layout.fillWidth: true; from: 0.1; to: 1; value: 0.9; onValueChanged: win.refreshPreview() }
                                            Label { Layout.preferredWidth: 38; text: inactiveOpacity.value.toFixed(2); font.family: "monospace" }
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: chromeContent.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: chromeContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 10
                                        Label { text: qsTr("Window decoration"); font.pixelSize: 15; font.bold: true }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            CheckBox { id: borderBox; text: qsTr("Set border width"); onToggled: win.refreshPreview() }
                                            Item { Layout.fillWidth: true }
                                            SpinBox { id: borderSize; from: 0; to: 20; value: 2; enabled: borderBox.checked; onValueChanged: win.refreshPreview() }
                                            Label { text: qsTr("px"); opacity: 0.55 }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            CheckBox { id: roundingBox; text: qsTr("Override corner rounding"); onToggled: win.refreshPreview() }
                                            Item { Layout.fillWidth: true }
                                            SpinBox { id: roundingField; from: 0; to: 100; value: 8; enabled: roundingBox.checked; onValueChanged: win.refreshPreview() }
                                            Label { text: qsTr("px"); opacity: 0.55 }
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: effectsContent.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: effectsContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 8
                                        Label { text: qsTr("Visual effects"); font.pixelSize: 15; font.bold: true }
                                        CheckBox { id: blurBox; text: qsTr("Disable background blur"); onToggled: win.refreshPreview() }
                                        CheckBox { id: shadowBox; text: qsTr("Disable window shadow"); onToggled: win.refreshPreview() }
                                    }
                                }
                            }
                        }

                        // BEHAVIOR
                        Item {
                            implicitHeight: behaviorContent.implicitHeight
                            ColumnLayout {
                                id: behaviorContent
                                width: parent.width
                                spacing: 12
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: focusContent.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: focusContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 8
                                        Label { text: qsTr("Focus & interaction"); font.pixelSize: 15; font.bold: true }
                                        CheckBox { id: focusBox; text: qsTr("Don't focus this window when it opens"); onToggled: win.refreshPreview() }
                                        Label { text: qsTr("Useful for notification windows and passive panels."); font.pixelSize: 11; opacity: 0.58; wrapMode: Text.WordWrap }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: performanceContent.implicitHeight + 30
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: performanceContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 8
                                        Label { text: qsTr("Performance & persistence"); font.pixelSize: 15; font.bold: true }
                                        CheckBox { id: animationBox; text: qsTr("Disable window animations"); onToggled: win.refreshPreview() }
                                        CheckBox { id: dimBox; text: qsTr("Disable dimming"); onToggled: win.refreshPreview() }
                                        CheckBox { id: vrrBox; text: qsTr("Disable variable refresh rate for this window"); onToggled: win.refreshPreview() }
                                        CheckBox { id: persistentSizeBox; text: qsTr("Remember floating size for matching windows"); onToggled: win.refreshPreview() }
                                    }
                                }
                            }
                        }

                        // SAVED RULES
                        Item {
                            implicitHeight: savedRulesContent.implicitHeight
                            ColumnLayout {
                                id: savedRulesContent
                                width: parent.width
                                spacing: 12
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: savedIntro.implicitHeight + 32
                                    radius: 13; color: win.surface; border.color: win.outline
                                    ColumnLayout {
                                        id: savedIntro
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 16; spacing: 5
                                        Label { text: qsTr("Saved rules"); font.pixelSize: 15; font.bold: true }
                                         Label {
                                             Layout.fillWidth: true
                                             text: qsTr("Manage persistent Hyprland window rules. Import and export the complete rule collection as JSON.")
                                             wrapMode: Text.WordWrap; font.pixelSize: 11; opacity: 0.6
                                         }
                                         TextField { id: savedRuleSearch; Layout.fillWidth: true; placeholderText: qsTr("Filter saved rules"); onTextChanged: win.visibleRules }
                                         RowLayout {
                                             Layout.fillWidth: true
                                             Button { text: qsTr("Import"); onClicked: { rulesDataDialog.importing = true; rulesDataDialog.open() } }
                                             Button { text: qsTr("Export"); onClicked: { rulesDataDialog.importing = false; rulesDataDialog.open() } }
                                         }
                                    }
                                }
                                Repeater {
                                     model: win.visibleRules
                                    delegate: Rectangle {
                                        required property var modelData
                                        required property int index
                                        Layout.fillWidth: true
                                        implicitHeight: ruleRow.implicitHeight + 26
                                        radius: 12; color: win.surface; border.color: win.outline
                                        RowLayout {
                                            id: ruleRow
                                            anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                                            anchors.leftMargin: 14; anchors.rightMargin: 10; spacing: 12
                                            Rectangle {
                                                Layout.preferredWidth: 34; Layout.preferredHeight: 34; radius: 9
                                                color: Qt.alpha(theme.accent, 0.15)
                                                Text { anchors.centerIn: parent; text: "R"; color: theme.accent; font.bold: true }
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true; spacing: 3
                                                Label {
                                                    Layout.fillWidth: true
                                                     text: modelData.name || modelData.title || modelData["class"]
                                                    font.pixelSize: 13; font.bold: true; elide: Text.ElideRight
                                                }
                                                 Label {
                                                     Layout.fillWidth: true
                                                     text: modelData["class"] + "  ·  " + (modelData.settings.layout || qsTr("floating")) + (modelData.enabled === false ? "  ·  " + qsTr("disabled") : "") + (modelData.conflict ? "  ·  ⚠ " + qsTr("overlap") : "")
                                                     font.pixelSize: 10; opacity: 0.58; elide: Text.ElideRight
                                                     ToolTip.visible: conflictHover.containsMouse && modelData.conflict
                                                     ToolTip.text: modelData.conflictsWith.join(", ")
                                                     MouseArea { id: conflictHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                                                 }
                                            }
                                             ToolButton {
                                                 text: modelData.enabled === false ? "○" : "●"
                                                 ToolTip.visible: hovered
                                                 ToolTip.text: modelData.enabled === false ? qsTr("Enable rule") : qsTr("Disable rule")
                                                 onClicked: backend.setRuleEnabled(modelData.sourceIndex, modelData.enabled === false)
                                             }
                                             ToolButton {
                                                 text: "▶"
                                                 ToolTip.visible: hovered
                                                 ToolTip.text: qsTr("Apply to matching open windows")
                                                 onClicked: backend.applySavedRule(modelData.sourceIndex)
                                             }
                                             ToolButton {
                                                 text: "⧉"
                                                 ToolTip.visible: hovered
                                                 ToolTip.text: qsTr("Duplicate rule")
                                                 onClicked: backend.duplicateRule(modelData.sourceIndex)
                                             }
                                             ToolButton {
                                                 text: "✎"
                                                 ToolTip.visible: hovered
                                                 ToolTip.text: qsTr("Edit rule in the main editor")
                                                 onClicked: win.loadRule(modelData.sourceIndex)
                                             }
                                             ToolButton {
                                                 text: "↑"
                                                 enabled: modelData.sourceIndex > 0
                                                 ToolTip.visible: hovered
                                                 ToolTip.text: qsTr("Move rule up")
                                                 onClicked: backend.moveRule(modelData.sourceIndex, modelData.sourceIndex - 1)
                                             }
                                             ToolButton {
                                                 text: "↓"
                                                 enabled: modelData.sourceIndex < backend.savedRules.length - 1
                                                 ToolTip.visible: hovered
                                                 ToolTip.text: qsTr("Move rule down")
                                                 onClicked: backend.moveRule(modelData.sourceIndex, modelData.sourceIndex + 1)
                                             }
                                             ToolButton {
                                                 text: "×"
                                                font.pixelSize: 20
                                                ToolTip.visible: hovered
                                                ToolTip.text: qsTr("Remove rule")
                                                 onClicked: { deleteRuleDialog.ruleIndex = modelData.sourceIndex; deleteRuleDialog.open() }
                                            }
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: backend.savedRules.length === 0
                                    text: qsTr("No saved rules yet. Choose a window and apply your first rule.")
                                    wrapMode: Text.WordWrap
                                    horizontalAlignment: Text.AlignHCenter
                                    opacity: 0.58
                                    padding: 28
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: previewExpanded ? 140 : 54
                    radius: 12
                    color: win.surface
                    border.color: win.outline
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 5
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("RULE PREVIEW"); font.pixelSize: 9; font.bold: true; color: theme.accent }
                            Item { Layout.fillWidth: true }
                            ToolButton { text: win.previewExpanded ? "⌄" : "⌃"; onClicked: win.previewExpanded = !win.previewExpanded }
                        }
                        TextArea {
                            id: preview
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: win.previewExpanded
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            font.family: "monospace"
                            font.pixelSize: 10
                            text: backend.previewRule(win.options())
                            background: Rectangle { color: win.raisedSurface; radius: 6 }
                        }
                        Label {
                            visible: !win.previewExpanded
                            Layout.fillWidth: true
                            text: backend.rulePath
                            elide: Text.ElideMiddle
                            font.pixelSize: 10
                            opacity: 0.56
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 66
                    radius: 12
                    color: win.surface
                    border.color: win.outline
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 12
                        spacing: 10
                        Rectangle {
                            Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4
                            color: backend.status.toLowerCase().indexOf("could not") >= 0 || backend.status.toLowerCase().indexOf("failed") >= 0 ? "#ff7474" : "#82d69b"
                        }
                        Label {
                            Layout.fillWidth: true
                            text: backend.status
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            font.pixelSize: 11
                            opacity: 0.78
                        }
                        Button {
                            text: win.editingRuleIndex >= 0 ? qsTr("Update rule") : qsTr("Apply rule")
                            highlighted: true
                            visible: win.category < 4
                            enabled: win.editingRuleIndex >= 0 || (backend.selectedIndex >= 0 && backend.windows.length > 0)
                            onClicked: {
                                if (win.editingRuleIndex >= 0) {
                                    if (backend.updateRule(win.editingRuleIndex, win.options())) win.editingRuleIndex = -1
                                } else backend.saveRule(win.options())
                            }
                        }
                        Button {
                            text: qsTr("Cancel edit")
                            visible: win.editingRuleIndex >= 0
                            onClicked: { win.editingRuleIndex = -1; win.refreshPreview() }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: refreshPreview()
}
