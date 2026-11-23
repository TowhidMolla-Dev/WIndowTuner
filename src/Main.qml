import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1120
    height: 820
    minimumWidth: Math.round(860 * textScale)
    minimumHeight: Math.round(680 * textScale)
    visible: true
    title: qsTr("WindowTuner")
    color: "#0b1014"

    // Omarchy Control-inspired shell; accents still follow the user's theme.
    readonly property color surface: "#11181d"
    readonly property color raisedSurface: "#182127"
    readonly property color outline: "#29343a"
    readonly property color mutedForeground: "#9aa6ad"
    readonly property var sections: [
        { title: qsTr("Layout"), icon: "▦", subtitle: qsTr("Shape and size this window") },
        { title: qsTr("Placement"), icon: "⌖", subtitle: qsTr("Choose where and how it appears") },
        { title: qsTr("Appearance"), icon: "◐", subtitle: qsTr("Tune transparency and surfaces") },
        { title: qsTr("Behavior"), icon: "⌘", subtitle: qsTr("Set focus and performance behavior") },
        { title: qsTr("Saved Rules"), icon: "☷", subtitle: qsTr("Manage persistent window rules") }
    ]

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: Material.Small
    font.pixelSize: 14
    property real textScale: backend.preference("textScale", 1.0)
    property bool advancedMode: backend.preference("advanced", false)
    onAdvancedModeChanged: if (editorReady) backend.setPreference("advanced", advancedMode)

    component ModuleHeading: RowLayout {
        id: heading
        property alias title: headingLabel.text
        property string explanation
        property string helpKey
        readonly property bool helpOpen: helpTip.opened
        objectName: "moduleHelp_" + helpKey
        Layout.fillWidth: true
        spacing: 8
        Label {
            id: headingLabel
            Layout.fillWidth: true
            font.pixelSize: 15
            font.bold: true
            wrapMode: Text.WordWrap
        }
        ToolButton {
            id: infoButton
            objectName: "moduleInfoButton"
            text: "ⓘ"
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            Accessible.name: qsTr("About %1").arg(heading.title)
            Accessible.description: heading.explanation
            onClicked: forceActiveFocus()
            Keys.onEscapePressed: focus = false
            contentItem: Label {
                text: infoButton.text
                color: infoButton.hovered || infoButton.activeFocus ? theme.accent : win.mutedForeground
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 6
                color: infoButton.hovered || infoButton.activeFocus ? win.raisedSurface : "transparent"
                border.color: infoButton.activeFocus ? theme.accent : "transparent"
            }
            ToolTip {
                id: helpTip
                parent: infoButton
                x: infoButton.width - width
                y: infoButton.height + 6
                margins: 12
                visible: infoButton.visible && (infoButton.hovered || infoButton.activeFocus)
                delay: 250
                timeout: -1
                width: Math.min(360, win.width - 48)
                text: heading.explanation
                contentItem: Label {
                    text: helpTip.text
                    wrapMode: Text.WordWrap
                    color: "#e0e8ef"
                    font.pixelSize: Math.round(12 * win.textScale)
                    lineHeight: 1.2
                }
                background: Rectangle { radius: 8; color: win.raisedSurface; border.color: win.outline }
            }
        }
    }

    component ToolLauncherTile: Button {
        required property string actionId
        required property string title
        required property string detail
        objectName: "toolLauncher_" + actionId
        Layout.fillWidth: true
        Layout.preferredHeight: 54
        hoverEnabled: true
        onClicked: win.openTool(actionId)
        background: Rectangle {
            radius: 4
            color: parent.hovered ? Qt.alpha(theme.accent, 0.08) : "transparent"
        }
        contentItem: RowLayout {
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Label {
                    Layout.fillWidth: true
                    text: parent.parent.parent.title
                    color: "#e6e9eb"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: parent.parent.parent.detail
                    color: win.mutedForeground
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
            Label { text: "›"; color: parent.parent.hovered ? theme.accent : win.mutedForeground; font.pixelSize: 18 }
        }
    }

    property string layoutMode: "floating"
    property int category: 0
    property bool windowsPage: true
    property bool desktopsPage: false
    property string windowFilter: ""
    property string ruleFilter: ""
    property int editingRuleIndex: -1
    property string editingRuleId: ""
    readonly property var editingRule: backend.savedRules.find(rule => rule.id === editingRuleId) || ({})
    readonly property bool editTargetAvailable: !editingRuleId || !!editingRule.id
    property bool previewExpanded: false
    property bool editorReady: false
    property string cleanDraft: ""
    readonly property bool dirty: editorReady && JSON.stringify(options()) !== cleanDraft
    property bool allowClose: false
    property int pendingWindow: -1
    property int pendingRule: -1
    property bool discardOnly: false
    property var editorMatch: ({})
    property bool groupApplications: true
    readonly property var fieldErrors: editorReady ? backend.validationDetails(options()) : ({})
    property bool settingsSearch: false
    property string settingsFilter: ""
    readonly property var settingIndex: [
        {title: qsTr("Window layout · floating, tiled, presets"), key: "layout", page: 0},
        {title: qsTr("Size & position · width, height, center"), key: "size", page: 0},
        {title: qsTr("Size constraints · minimum, maximum"), key: "constraints", page: 0, advanced: true},
        {title: qsTr("Workspace & visibility · pin"), key: "workspace", page: 1},
        {title: qsTr("Rule matching · class, title, regex"), key: "matching", page: 1, advanced: true},
        {title: qsTr("Opacity · transparency"), key: "opacity", page: 2},
        {title: qsTr("Window decoration · border, rounding"), key: "decoration", page: 2},
        {title: qsTr("Visual effects · blur, shadow"), key: "effects", page: 2},
        {title: qsTr("Focus & interaction"), key: "focus", page: 3},
        {title: qsTr("Performance & persistence · animations, VRR"), key: "performance", page: 3},
        {title: qsTr("Saved rules · organize, import, export"), key: "saved", page: 4}
    ]
    readonly property var settingResults: settingIndex.filter(item => item.title.toLocaleLowerCase().indexOf(settingsFilter.toLocaleLowerCase()) >= 0)
    function jumpToSetting(result) {
        if (result.advanced) advancedMode = true
        navigate(result.page)
        settingsResultsPopup.close()
        function locate(parent) {
            if (parent.objectName === "moduleHelp_" + result.key) return parent
            for (let i = 0; i < parent.children.length; ++i) {
                const found = locate(parent.children[i])
                if (found) return found
            }
            return null
        }
        Qt.callLater(function() {
            const heading = locate(win.contentItem)
            if (heading && settingsScroll.contentItem.contentItem)
                settingsScroll.contentItem.contentY = Math.max(0, heading.mapToItem(settingsScroll.contentItem.contentItem, 0, 0).y - 16)
        })
    }

    function openTool(actionId) {
        toolsMenu.close()
        switch (actionId) {
        case "safety": safetyDialog.open(); break
        case "placement": placementDesigner.open(); break
        case "inspect": inspectorDialog.open(); break
        case "workflows": workflowDialog.open(); break
        case "management": managementDialog.open(); break
        case "appspace": appSpaceDialog.open(); break
        case "health": healthDialog.open(); break
        case "welcome": welcomeDialog.step = 0; welcomeDialog.open(); break
        case "shortcuts": shortcutsDialog.open(); break
        }
    }
    function markClean() { cleanDraft = JSON.stringify(options()) }
    function chooseWindow(index) {
        if (dirty) { pendingWindow = index; pendingRule = -1; discardOnly = false; discardDialog.open(); return }
        backend.selectWindow(index); editingRuleIndex = -1; editingRuleId = ""; editorMatch = ({})
        classPatternField.text = backend.selectedWindow.currentClass || backend.selectedWindow["class"] || ""
        titlePatternField.text = backend.selectedWindow.currentTitle || backend.selectedWindow.title || ""
        navigate(0); markClean()
    }
    function requestSave() {
        if (!editTargetAvailable) { missingRuleNotice.open(); return }
        saveReview.pending = options()
        saveReview.ruleIndex = editingRuleId ? backend.savedRules.findIndex(rule => rule.id === editingRuleId) : -1
        saveReview.review = backend.reviewChanges(saveReview.pending, saveReview.ruleIndex)
        saveReview.open()
    }
    StudioDialog {
        id: missingRuleNotice
        anchors.centerIn: parent; modal: true
        title: qsTr("The rule being edited no longer exists")
        standardButtons: Dialog.Close
        Label { width: 360; wrapMode: Text.WordWrap; text: qsTr("No other rule will be updated. Cancel edit or choose a target to deliberately create a new rule.") }
    }
    function discardCurrentDraft() { pendingWindow = -1; pendingRule = -1; discardOnly = true; discardDialog.open() }
    onClosing: function(close) {
        if (backend.trialActive && !backend.revertTrial()) { close.accepted = false; return }
        if (dirty && !allowClose) { close.accepted = false; pendingWindow = -1; pendingRule = -1; discardOnly = false; discardDialog.open() }
    }
    StudioDialog {
        id: discardDialog
        objectName: "discardDialog"
        anchors.centerIn: parent
        modal: true
        title: qsTr("Unsaved changes")
        standardButtons: Dialog.Discard | Dialog.Cancel
        Label { width: 360; wrapMode: Text.WordWrap; text: qsTr("Discard this draft? Your saved rules will not be changed.") }
        onDiscarded: {
            win.applySettings(JSON.parse(win.cleanDraft))
            win.markClean()
            if (win.discardOnly) { win.discardOnly = false; return }
            if (win.pendingRule >= 0) win.loadRule(win.pendingRule)
            else if (win.pendingWindow >= 0) win.chooseWindow(win.pendingWindow)
            else { win.allowClose = true; win.close() }
        }
    }
    StudioDialog {
        id: saveReview
        objectName: "saveReview"
        property var pending: ({})
        property var review: ({})
        property var verification: ({})
        property int ruleIndex: -1
        anchors.centerIn: parent
        width: Math.min(win.width - 60, 720)
        height: Math.min(win.height - 60, 620)
        modal: true
        title: qsTr("Review before applying")
        standardButtons: Dialog.Save | Dialog.Cancel
        onOpened: {
            verification = backend.verifyDraft(pending)
            standardButton(Dialog.Save).enabled = (review.errors || []).length === 0 && verification.ok === true
        }
        contentItem: ScrollView {
            clip: true
            TextArea {
                objectName: "changeReviewText"
                readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
                text: (saveReview.review.errors || []).join("\n") + "\n" + (saveReview.review.summary || "") +
                      "\n\n" + qsTr("Installed-parser validation: %1").arg(saveReview.verification.status || "unverified") +
                      (saveReview.verification.ok ? "" : "\n" + (saveReview.verification.details || "")) +
                      "\n\n" + (saveReview.review.diff || "") + "\n\n" +
                      qsTr("Main config: WindowTuner may add its module include. Reload validation runs after saving.")
            }
        }
        onAccepted: {
            const ok = ruleIndex >= 0 ? backend.updateRule(ruleIndex, pending) : backend.saveRule(pending)
            if (ok) { win.editingRuleIndex = -1; win.editingRuleId = ""; win.markClean() }
        }
    }
    SafetyDialog { id: safetyDialog; host: win }
    PlacementDesigner { id: placementDesigner; host: win }
    InspectorDialog { id: inspectorDialog; host: win }
    WorkflowDialog { id: workflowDialog; host: win }
    ManagementDialog { id: managementDialog; host: win }
    AppSpaceDialog { id: appSpaceDialog; host: win }
    ScenesDialog { id: scenesDialog; host: win }
    HealthDialog { id: healthDialog; host: win }
    WelcomeDialog { id: welcomeDialog; host: win }
    StudioDialog {
        id: shortcutsDialog
        objectName: "shortcutsDialog"
        anchors.centerIn: parent; modal: true; title: qsTr("Keyboard shortcuts")
        standardButtons: Dialog.Close
        Label { width: 440; wrapMode: Text.WordWrap; text: qsTr("Ctrl+S — review and save\nCtrl+K / Ctrl+F — focus search\nCtrl+R — refresh windows\nAlt+1–5 — settings sections\nCtrl+Z / Ctrl+Shift+Z — configuration undo/redo when draft is clean\nTab / Shift+Tab — move focus\nEscape — dismiss dialogs or leave edit mode\n? — this help\nCtrl+Q — quit (asks about unsaved changes)") }
    }
    function startOfflineTarget(withTitle) {
        editingRuleIndex = -1; editingRuleId = ""; editorMatch = ({}); classPatternField.text = ""; titlePatternField.text = ""
        titleMatch.checked = withTitle; navigate(1); markClean()
    }
    property var visibleWindows: {
        const needle = windowFilter.trim().toLocaleLowerCase()
        const rows = backend.windows
        const result = []
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            const searchable = (row.currentTitle + " " + row.title + " " + row["class"] + " " + row.workspace).toLocaleLowerCase()
            if (!needle || searchable.indexOf(needle) !== -1) {
                const item = Object.assign({}, row)
                item.sourceIndex = i
                item.appGroup = row["class"] || ""
                result.push(item)
            }
        }
        if (groupApplications) result.sort((a, b) => a.appGroup.localeCompare(b.appGroup))
        return result
    }
    property var visibleRules: {
        const needle = ruleFilter.trim().toLocaleLowerCase()
        const result = []
        const rows = backend.savedRules
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            const text = ((row.name || "") + " " + (row["class"] || "") + " " + (row.title || "") + " " + (row.folder || "") + " " + (row.tags || []).join(" ")).toLocaleLowerCase()
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
        const match = Object.assign({}, editorMatch)
        if (target["class"]) match["class"] = classPatternField.text || editorMatch["class"] || target["class"]
        function condition(key, enabled, value) {
            if (!enabled) delete match[key]
            else if (editorMatch[key] !== undefined) match[key] = editorMatch[key]
            else if (value !== undefined && value !== "") match[key] = value
        }
        condition("title", titleMatch.checked, titlePatternField.text || target.title)
        if (titleMatch.checked && titlePatternField.text) match.title = titlePatternField.text
        condition("workspace", workspaceMatch.checked, target.workspace)
        condition("xdg_tag", xdgTagMatch.checked, target.xdgTag)
        condition("xwayland", xwaylandMatch.checked, !!target.xwayland)
        condition("fullscreen", fullscreenMatch.checked, target.fullscreen !== 0)
        condition("group", groupMatch.checked, !!target.grouped)
        condition("content", contentMatch.checked, target.contentType)
        return backend.resolvePlacement({
            placement: placementDesigner.placement,
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
        })
    }

    function refreshPreview() { preview.text = backend.previewRule(options()) }
    function navigate(section) {
        desktopsPage = section === -2
        windowsPage = section === -1
        if (section >= 0) category = section
        headerSearch.text = !windowsPage && !desktopsPage && category === 4 ? ruleFilter : windowFilter
    }
    function loadRule(index) {
        if (dirty) { pendingRule = index; pendingWindow = -1; discardOnly = false; discardDialog.open(); return }
        const saved = backend.loadRuleForEditing(index)
        if (!saved || Object.keys(saved).length === 0) return
        navigate(0)
        editingRuleIndex = index
        editingRuleId = backend.savedRules[index].id || ""
        applySettings(saved)
        markClean()
    }
    function restorePreset(settings) {
        applySettings(Object.assign({}, options(), settings))
        refreshPreview()
    }
    function applySettings(saved) {
        const match = saved.match || {}
        editorMatch = match
        classPatternField.text = match["class"] || ""
        titlePatternField.text = match.title || ""
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
        borderSize.value = saved.borderSize === undefined ? 2 : saved.borderSize
        workspaceField.text = saved.workspace || ""
        placementDesigner.placement = saved.placement || {enabled: false}
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

    StudioDialog {
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

    StudioDialog {
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
        onActivated: win.requestSave()
    }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: { headerSearch.forceActiveFocus(); headerSearch.selectAll() } }
    Shortcut { sequence: "Ctrl+K"; context: Qt.ApplicationShortcut; onActivated: { headerSearch.forceActiveFocus(); headerSearch.selectAll() } }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: win.editingRuleIndex >= 0; onActivated: { win.editingRuleIndex = -1; win.editingRuleId = "" } }
    Shortcut { sequence: "Alt+1"; context: Qt.ApplicationShortcut; onActivated: win.navigate(0) }
    Shortcut { sequence: "Alt+2"; context: Qt.ApplicationShortcut; onActivated: win.navigate(1) }
    Shortcut { sequence: "Alt+3"; context: Qt.ApplicationShortcut; onActivated: win.navigate(2) }
    Shortcut { sequence: "Alt+4"; context: Qt.ApplicationShortcut; onActivated: win.navigate(3) }
    Shortcut { sequence: "Alt+5"; context: Qt.ApplicationShortcut; onActivated: win.navigate(4) }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: !win.dirty; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo]; context: Qt.ApplicationShortcut; enabled: !win.dirty; onActivated: backend.redo() }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: shortcutsDialog.open() }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }

    ColumnLayout {
        width: win.width / win.textScale
        height: win.height / win.textScale
        scale: win.textScale
        transformOrigin: Item.TopLeft
        spacing: 0

        // A restrained app bar separates the product identity from the task.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "#0e1418"
            border.color: win.outline
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 20
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    color: "transparent"
                    Text { anchors.centerIn: parent; text: "☷"; color: theme.accent; font.pixelSize: 22 }
                }
                ColumnLayout {
                    spacing: 1
                    Layout.preferredWidth: 145
                    Label { text: qsTr("WindowTuner"); font.pixelSize: 14; font.bold: true }
                }
                TextField {
                    id: headerSearch
                    objectName: "headerSearch"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    selectByMouse: true
                    leftPadding: 12
                    rightPadding: 68
                    background: Rectangle { radius: 6; color: "#0b141b"; border.color: headerSearch.activeFocus ? theme.accent : win.outline }
                    onTextEdited: {
                        if (win.settingsSearch) { win.settingsFilter = text; settingsResultsPopup.open() }
                        else if (!win.desktopsPage && !win.windowsPage && win.category === 4) win.ruleFilter = text
                        else { win.windowFilter = text; win.desktopsPage = false; win.windowsPage = true }
                    }
                    Label {
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.right: parent.right; anchors.rightMargin: 68
                        anchors.verticalCenter: parent.verticalCenter
                        visible: headerSearch.text.length === 0
                        text: win.settingsSearch ? qsTr("Search settings…") : !win.desktopsPage && !win.windowsPage && win.category === 4 ? qsTr("Search saved rules…") : qsTr("Search windows by app, title, or workspace…")
                        color: win.mutedForeground; font.pixelSize: 12; elide: Text.ElideRight
                    }
                    Label { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Ctrl K"; color: win.mutedForeground; font.pixelSize: 10 }
                    Accessible.name: qsTr("Search windows or saved rules")
                    Popup {
                        id: settingsResultsPopup
                        objectName: "settingsResultsPopup"
                        y: headerSearch.height + 6
                        width: headerSearch.width
                        padding: 8
                        contentItem: ColumnLayout {
                            Repeater {
                                model: win.settingResults
                                delegate: ItemDelegate { required property var modelData; text: modelData.title; Layout.fillWidth: true; onClicked: win.jumpToSetting(modelData) }
                            }
                            Label { visible: win.settingResults.length === 0; text: qsTr("No matching settings") }
                        }
                    }
                }
                ToolButton {
                    objectName: "settingsSearchToggle"
                    text: win.settingsSearch ? qsTr("Switch to windows") : qsTr("Switch to settings")
                    checkable: true
                    checked: win.settingsSearch
                    Accessible.name: win.settingsSearch ? qsTr("Search windows instead of settings") : qsTr("Search settings instead of windows")
                    ToolTip.visible: hovered
                    ToolTip.text: win.settingsSearch ? qsTr("Switch search to windows") : qsTr("Switch search to settings")
                    onClicked: { win.settingsSearch = !win.settingsSearch; headerSearch.text = ""; headerSearch.forceActiveFocus() }
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
            spacing: 0

            Rectangle {
                Layout.preferredWidth: win.width < 1000 ? 178 : 210
                Layout.fillHeight: true
                color: "#080e12"
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: win.outline }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 5
                    Button {
                        objectName: "windowsNavigation"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        text: qsTr("▦   Windows")
                        flat: true
                        highlighted: win.windowsPage
                        onClicked: win.navigate(-1)
                        background: Rectangle { radius: 4; color: win.windowsPage ? Qt.alpha(theme.accent, 0.08) : parent.hovered ? win.raisedSurface : "transparent" }
                        contentItem: RowLayout {
                            Rectangle { Layout.preferredWidth: 2; Layout.fillHeight: true; color: win.windowsPage ? theme.accent : "transparent" }
                            Label { Layout.fillWidth: true; text: parent.parent.text; color: win.windowsPage ? "#f1f3f4" : "#c2cdd6"; verticalAlignment: Text.AlignVCenter; leftPadding: 10 }
                        }
                    }
                    Button {
                        objectName: "desktopsNavigation"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        text: qsTr("▦   Desktops")
                        flat: true
                        highlighted: win.desktopsPage
                        onClicked: win.navigate(-2)
                        background: Rectangle {
                            radius: 4
                            color: win.desktopsPage ? Qt.alpha(theme.accent, 0.08)
                                : parent.hovered ? win.raisedSurface : "transparent"
                        }
                        contentItem: RowLayout {
                            Rectangle {
                                Layout.preferredWidth: 2
                                Layout.fillHeight: true
                                color: win.desktopsPage ? theme.accent : "transparent"
                            }
                            Label {
                                Layout.fillWidth: true
                                text: parent.parent.text
                                color: win.desktopsPage ? "#f1f3f4" : "#c2cdd6"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                            }
                        }
                        Accessible.name: qsTr("Desktop overview")
                    }
                    Rectangle { Layout.fillWidth: true; Layout.topMargin: 6; Layout.bottomMargin: 6; height: 1; color: win.outline }
                    Label { text: qsTr("Rule editor"); font.pixelSize: 11; font.weight: Font.Medium; color: win.mutedForeground; leftPadding: 10; Layout.bottomMargin: 5 }
                    Repeater {
                        model: win.sections
                        delegate: Button {
                            id: navButton
                            required property var modelData
                            required property int index
                            objectName: "sectionNavigation" + index
                            readonly property bool selected: !win.windowsPage && !win.desktopsPage && win.category === index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            flat: true
                            onClicked: win.navigate(index)
                            background: Rectangle {
                                radius: 4
                                color: navButton.selected ? Qt.alpha(theme.accent, 0.08) : navButton.hovered ? win.raisedSurface : "transparent"
                            }
                            contentItem: RowLayout {
                                spacing: 10
                                Rectangle { Layout.preferredWidth: 2; Layout.fillHeight: true; color: navButton.selected ? theme.accent : "transparent" }
                                Label { text: modelData.icon; color: navButton.selected ? theme.accent : win.mutedForeground; Layout.preferredWidth: 22; horizontalAlignment: Text.AlignHCenter }
                                Label { text: modelData.title; color: navButton.selected ? "#f1f3f4" : "#c2cdd6"; Layout.fillWidth: true; elide: Text.ElideRight }
                            }
                            Accessible.name: modelData.title
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Button {
                        id: toolsNavigation
                        objectName: "toolsNavigation"
                        text: qsTr("Tools and workflows")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        hoverEnabled: true
                        flat: true
                        onClicked: toolsMenu.open()
                        background: Rectangle {
                            radius: 4
                            color: toolsMenu.opened ? Qt.alpha(theme.accent, 0.08)
                                : toolsNavigation.hovered ? win.raisedSurface : "transparent"
                            border.color: "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: 10
                            Label {
                                text: "⌘"
                                color: toolsMenu.opened ? theme.accent : win.mutedForeground
                                font.pixelSize: 17
                                Layout.preferredWidth: 22
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Label {
                                Layout.fillWidth: true
                                text: toolsNavigation.text
                                color: toolsMenu.opened ? "#f1f3f4" : "#c2cdd6"
                            }
                            Label { text: toolsMenu.opened ? "⌃" : "›"; color: win.mutedForeground; font.pixelSize: 16 }
                        }
                        Popup {
                            id: toolsMenu
                            objectName: "toolsMenu"
                            parent: Overlay.overlay
                            x: {
                                const p = toolsNavigation.mapToItem(Overlay.overlay, 0, 0)
                                return Math.max(12, Math.min(win.width - width - 12, p.x))
                            }
                            y: {
                                const p = toolsNavigation.mapToItem(Overlay.overlay, 0, 0)
                                return Math.max(12, Math.min(win.height - height - 12, p.y - height - 10))
                            }
                            width: Math.min(500, win.width - 24)
                            height: Math.min(implicitHeight, win.height - 24)
                            padding: 14
                            modal: false
                            focus: true
                            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                            background: Rectangle {
                                radius: 7
                                color: "#11181d"
                                border.color: win.outline
                            }
                            contentItem: ColumnLayout {
                                id: toolsLauncherContent
                                width: toolsMenu.availableWidth
                                spacing: 10
                                RowLayout {
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label { text: qsTr("Tools and workflows"); font.pixelSize: 17; font.weight: Font.Medium }
                                        Label { text: qsTr("Utilities for rules, placement, and diagnostics"); color: win.mutedForeground; font.pixelSize: 11 }
                                    }
                                    ToolButton {
                                        text: "×"
                                        Accessible.name: qsTr("Close tools and workflows")
                                        onClicked: toolsMenu.close()
                                    }
                                }
                                Rectangle { Layout.fillWidth: true; height: 1; color: win.outline }
                                Label {
                                    text: qsTr("Rule tools")
                                    color: win.mutedForeground
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    Layout.topMargin: 1
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    rowSpacing: 7
                                    columnSpacing: 7
                                    ToolLauncherTile { actionId: "safety"; title: qsTr("Safety and history"); detail: qsTr("Undo, snapshots, and trial changes") }
                                    ToolLauncherTile { actionId: "placement"; title: qsTr("Placement designer"); detail: qsTr("Arrange a window visually") }
                                    ToolLauncherTile { actionId: "inspect"; title: qsTr("Inspect match"); detail: qsTr("See which rules match a window") }
                                    ToolLauncherTile { actionId: "workflows"; title: qsTr("Presets and profiles"); detail: qsTr("Save and switch rule collections") }
                                }
                                Label {
                                    text: qsTr("Applications and placement")
                                    color: win.mutedForeground
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    Layout.topMargin: 1
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    rowSpacing: 7
                                    columnSpacing: 7
                                    ToolLauncherTile { actionId: "management"; title: qsTr("Applications and imports"); detail: qsTr("Create targets and organize rules") }
                                    ToolLauncherTile { objectName: "appSpaceMenuEntry"; actionId: "appspace"; title: qsTr("Apps, workspaces, displays"); detail: qsTr("Choose where apps open") }
                                }
                                Label {
                                    text: qsTr("Help and diagnostics")
                                    color: win.mutedForeground
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    Layout.topMargin: 1
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    rowSpacing: 7
                                    columnSpacing: 7
                                    ToolLauncherTile { actionId: "health"; title: qsTr("Compatibility and health"); detail: qsTr("Check supported Hyprland features") }
                                    ToolLauncherTile { actionId: "welcome"; title: qsTr("Getting started"); detail: qsTr("Open the guided walkthrough") }
                                    ToolLauncherTile { actionId: "shortcuts"; title: qsTr("Keyboard shortcuts"); detail: qsTr("View available key commands") }
                                }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: win.outline }
                    Label { text: qsTr("Hyprland window rules"); color: win.mutedForeground; font.pixelSize: 10; leftPadding: 10; Layout.topMargin: 8 }
                    Label { text: qsTr("Alt 1–5  ·  Switch section"); color: win.mutedForeground; font.pixelSize: 9; leftPadding: 10 }
                }
            }

            // A dedicated discovery page keeps the settings pane uncluttered.
            Rectangle {
                objectName: "windowsPage"
                visible: win.windowsPage && !win.desktopsPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 20
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Label { text: qsTr("Windows"); font.pixelSize: 25; font.bold: true }
                            Label { text: qsTr("Choose a window to configure its rules."); color: win.mutedForeground; font.pixelSize: 12 }
                        }
                        Item { Layout.fillWidth: true }
                        CheckBox { text: qsTr("Group apps"); checked: win.groupApplications; onToggled: win.groupApplications = checked }
                        Button {
                            objectName: "setupOfflineRuleAction"
                            text: qsTr("Add offline app rule…")
                            flat: true
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Create a rule for an app that may not be open")
                            onClicked: managementDialog.open()
                        }
                        ToolButton {
                            text: "⌖"
                            font.pixelSize: 19
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Pick a window on screen")
                            onClicked: pickerOverlay.visible = true
                        }
                        Label {
                            text: qsTr("%1 open").arg(backend.windows.length)
                            color: win.mutedForeground
                            font.pixelSize: 11
                        }
                    }

                    ListView {
                        id: windowList
                        objectName: "windowList"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 0
                        model: win.visibleWindows
                        section.property: win.groupApplications ? "appGroup" : ""
                        section.delegate: Label { required property string section; text: section; color: theme.accent; padding: 8; font.bold: true }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: ItemDelegate {
                            id: windowItem
                            required property var modelData
                            objectName: "windowEntry" + modelData.sourceIndex
                            width: ListView.view.width
                            height: modelData.sourceIndex === backend.selectedIndex ? 84 : 62
                            highlighted: modelData.sourceIndex === backend.selectedIndex
                            onClicked: win.chooseWindow(modelData.sourceIndex)
                            background: Rectangle {
                                radius: 3
                                color: windowItem.highlighted ? Qt.alpha(theme.accent, 0.08) : (windowItem.hovered ? Qt.alpha("#ffffff", 0.035) : "transparent")
                                border.width: 0
                                Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 2; color: windowItem.highlighted ? theme.accent : "transparent" }
                                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: win.outline; opacity: 0.65 }
                            }
                            contentItem: RowLayout {
                                spacing: 12
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData["class"] + "  —  " + (modelData.currentTitle || modelData.title || qsTr("Untitled window"))
                                        elide: Text.ElideRight
                                        font.pixelSize: 13
                                        font.weight: windowItem.highlighted ? Font.Medium : Font.Normal
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("WS %1  ·  %2").arg(modelData.workspace).arg(modelData.monitorName || qsTr("Monitor %1").arg(modelData.monitor))
                                        elide: Text.ElideRight
                                        color: win.mutedForeground
                                        font.pixelSize: 11
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: windowItem.highlighted
                                        text: qsTr("PID %1  ·  %2  ·  %3 × %4  ·  %5  ·  %6")
                                            .arg(modelData.pid)
                                            .arg(modelData.address)
                                            .arg(modelData.size[0])
                                            .arg(modelData.size[1])
                                            .arg(modelData.fullscreen ? qsTr("fullscreen") : modelData.floating ? qsTr("floating") : qsTr("tiled"))
                                            .arg(modelData.xwayland ? "XWayland" : "Wayland")
                                        elide: Text.ElideRight
                                        color: win.mutedForeground
                                        font.pixelSize: 10
                                    }
                                }
                                Rectangle {
                                    visible: modelData.floating
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 20
                                    radius: 3
                                    color: Qt.alpha("#79d49b", 0.1)
                                    Label { anchors.centerIn: parent; text: qsTr("FLOAT"); color: "#79d49b"; font.pixelSize: 9; font.weight: Font.Medium }
                                    ToolTip.visible: floatingHint.hovered
                                    ToolTip.text: qsTr("Currently floating")
                                    HoverHandler { id: floatingHint }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            visible: win.visibleWindows.length === 0
                            spacing: 10
                            Label {
                                Layout.fillWidth: true
                                text: backend.windows.length === 0 ? qsTr("No windows are open") : qsTr("No windows match this search")
                                color: win.mutedForeground
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Button {
                                objectName: "clearWindowSearchAction"
                                visible: backend.windows.length > 0
                                Layout.alignment: Qt.AlignHCenter
                                text: qsTr("Clear search")
                                onClicked: headerSearch.text = ""
                            }
                        }
                    }

                }
            }

            ColumnLayout {
                objectName: "desktopsPage"
                visible: win.desktopsPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 20
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: qsTr("Desktops"); font.pixelSize: 25; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("See what is open and which apps have placement rules. This view does not move windows.")
                        color: win.mutedForeground
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        Button {
                            objectName: "desktopRefreshAction"
                            text: qsTr("Refresh")
                            onClicked: backend.refreshWindows()
                        }
                        Button {
                            objectName: "desktopManageAppsAction"
                            text: qsTr("Manage apps")
                            onClicked: appSpaceDialog.open()
                        }
                        Button {
                            objectName: "desktopScenesAction"
                            text: qsTr("Saved scenes")
                            onClicked: scenesDialog.open()
                        }
                    }
                }

                ScrollView {
                    id: desktopOverviewScroll
                    objectName: "desktopOverviewScroll"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        width: desktopOverviewScroll.availableWidth
                        spacing: 12

                        Label {
                            objectName: "desktopNoMonitorData"
                            visible: backend.monitors.length === 0
                            Layout.fillWidth: true
                            text: qsTr("Hyprland monitor data is unavailable. Refresh to try again.")
                            color: win.mutedForeground
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: backend.monitors
                            delegate: Rectangle {
                                id: monitorOverviewCard
                                required property var modelData
                                required property int index
                                objectName: "desktopMonitorCard" + index
                                Layout.fillWidth: true
                                implicitHeight: monitorOverview.implicitHeight + 24
                                radius: 8
                                color: win.surface
                                border.color: win.outline

                                ColumnLayout {
                                    id: monitorOverview
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 10

                                    RowLayout {
                                        Layout.fillWidth: true
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Label {
                                                text: monitorOverviewCard.modelData.name || qsTr("Display %1").arg(monitorOverviewCard.index + 1)
                                                font.pixelSize: 16
                                                font.bold: true
                                            }
                                            Label {
                                                text: monitorOverviewCard.modelData.description || qsTr("Display identity unavailable")
                                                color: win.mutedForeground
                                                font.pixelSize: 11
                                                elide: Text.ElideMiddle
                                            }
                                        }
                                        Label {
                                            text: qsTr("%1×%2 · %3%")
                                                .arg(monitorOverviewCard.modelData.width || 0)
                                                .arg(monitorOverviewCard.modelData.height || 0)
                                                .arg(Math.round((monitorOverviewCard.modelData.scale || 1) * 100))
                                            color: win.mutedForeground
                                            font.pixelSize: 11
                                        }
                                    }

                                    GridLayout {
                                        id: workspaceGrid
                                        Layout.fillWidth: true
                                        columns: width < 620 ? 1 : 2
                                        rowSpacing: 8
                                        columnSpacing: 8

                                        Repeater {
                                            model: backend.workspacePlan
                                            delegate: Rectangle {
                                                required property var modelData
                                                readonly property string slot: modelData.slot
                                                readonly property string monitorDescription: monitorOverviewCard.modelData.description || ""
                                                readonly property string activeWorkspaceName:
                                                    monitorOverviewCard.modelData.activeWorkspace
                                                        ? String(monitorOverviewCard.modelData.activeWorkspace.name || "")
                                                        : ""
                                                readonly property var openWindows: backend.windows.filter(window =>
                                                    window.monitorName === monitorOverviewCard.modelData.name
                                                    && window.workspace === slot)
                                                Layout.fillWidth: true
                                                implicitHeight: workspaceTileContent.implicitHeight + 20
                                                objectName: "desktopWorkspace" + monitorOverviewCard.index + "-" + slot
                                                radius: 6
                                                color: activeWorkspaceName === slot
                                                    ? Qt.alpha(theme.accent, 0.1) : win.raisedSurface
                                                border.color: activeWorkspaceName === slot
                                                    ? Qt.alpha(theme.accent, 0.55) : win.outline
                                                visible: modelData.monitor === "" || modelData.monitor === monitorDescription
                                                    || openWindows.length > 0 || activeWorkspaceName === slot

                                                ColumnLayout {
                                                    id: workspaceTileContent
                                                    anchors.fill: parent
                                                    anchors.margins: 10
                                                    spacing: 5
                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        Label {
                                                            text: qsTr("Workspace %1").arg(slot)
                                                            font.weight: Font.Medium
                                                        }
                                                        Item { Layout.fillWidth: true }
                                                        Label {
                                                            objectName: "desktopWorkspaceActive" + monitorOverviewCard.index + "-" + slot
                                                            visible: activeWorkspaceName === slot
                                                            text: qsTr("ACTIVE")
                                                            color: theme.accent
                                                            font.pixelSize: 10
                                                            font.bold: true
                                                        }
                                                        Label {
                                                            visible: modelData.monitor === monitorDescription && monitorDescription !== ""
                                                            text: qsTr("PINNED")
                                                            color: win.mutedForeground
                                                            font.pixelSize: 10
                                                        }
                                                    }
                                                    Label {
                                                        objectName: "desktopWorkspaceOpenApps" + monitorOverviewCard.index + "-" + slot
                                                        Layout.fillWidth: true
                                                        text: openWindows.length > 0
                                                            ? openWindows.map(window => window.currentTitle || window["class"]).join(", ")
                                                            : qsTr("No open windows")
                                                        color: openWindows.length > 0 ? "#d5dde2" : win.mutedForeground
                                                        font.pixelSize: 11
                                                        wrapMode: Text.WordWrap
                                                    }
                                                    Label {
                                                        Layout.fillWidth: true
                                                        visible: (modelData.apps || []).length > 0
                                                        text: qsTr("Placement rules: %1").arg((modelData.apps || []).join(", "))
                                                        color: win.mutedForeground
                                                        font.pixelSize: 10
                                                        wrapMode: Text.WordWrap
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                objectName: "editorPage"
                visible: !win.windowsPage && !win.desktopsPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 20
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 2
                        Label { text: win.sections[win.category].title; font.pixelSize: 23; font.bold: true }
                        Label { text: win.sections[win.category].subtitle; color: win.mutedForeground; font.pixelSize: 11 }
                    }
                    Item { Layout.fillWidth: true }
                    ComboBox { objectName: "editorMode"; model: [qsTr("Basic"), qsTr("Advanced")]; currentIndex: win.advancedMode ? 1 : 0; onActivated: win.advancedMode = currentIndex === 1 }
                    Button {
                        text: qsTr("Change window")
                        flat: true
                        onClicked: win.navigate(-1)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    radius: 0
                    color: "transparent"
                    border.color: "transparent"
                    Rectangle { anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; height: 1; color: win.outline }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 2
                        anchors.rightMargin: 4
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        spacing: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Label {
                                text: win.editingRuleIndex >= 0 ? qsTr("Editing saved rule") : qsTr("Selected window")
                                color: win.mutedForeground; font.pixelSize: 10; font.weight: Font.Medium
                            }
                            Label {
                                Layout.fillWidth: true
                                text: win.editingRuleId ? (win.editingRule.name || qsTr("Edited rule was removed")) : backend.selectedWindow["class"] ? (backend.selectedWindow.currentTitle || backend.selectedWindow.title || backend.selectedWindow["class"]) : qsTr("Choose a window to get started")
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: backend.selectedIndex >= 0
                                    ? backend.selectedWindow["class"] + "  ·  WS " + backend.selectedWindow.workspace + "  ·  " + (backend.selectedWindow.monitorName || qsTr("Monitor %1").arg(backend.selectedWindow.monitor))
                                    : qsTr("Use Windows in the sidebar to choose a target.")
                                color: win.mutedForeground; font.pixelSize: 10; elide: Text.ElideRight
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
                                     implicitHeight: layoutCard.implicitHeight + 24
                                     radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: layoutCard
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                         anchors.margins: 12; spacing: 14
                                        ColumnLayout {
                                            spacing: 3
                                            ModuleHeading {
                                                title: qsTr("Window layout"); helpKey: "layout"
                                                explanation: qsTr("Choose how matching windows are arranged. Floating allows a custom size and position; Tiled lets the workspace layout manage them. Keep current leaves the layout mode unchanged. Quick presets fill in common sizes and positions.")
                                            }
                                            Label { text: qsTr("Control how this window joins your workspace."); font.pixelSize: 11; opacity: 0.58 }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8
                                            Repeater {
                                                model: [qsTr("Floating"), qsTr("Keep current"), qsTr("Tiled")]
                                                delegate: Button {
                                                    id: layoutChoice
                                                    required property string modelData
                                                    required property int index
                                                    Layout.fillWidth: true
                                                    text: modelData
                                                    checkable: true
                                                    checked: ["floating", "keep", "tiled"][index] === win.layoutMode
                                                    onClicked: { win.layoutMode = ["floating", "keep", "tiled"][index]; win.refreshPreview() }
                                                    background: Rectangle { radius: 6; color: layoutChoice.checked ? Qt.alpha(theme.accent, 0.13) : win.raisedSurface; border.color: layoutChoice.checked ? Qt.alpha(theme.accent, 0.45) : win.outline }
                                                }
                                            }
                                        }
                                        Label { text: qsTr("Quick presets"); font.pixelSize: 11; opacity: 0.65 }
                                        Flow {
                                            Layout.fillWidth: true
                                            spacing: 7
                                             Button {
                                                 id: compactPreset
                                                 text: qsTr("Compact · 500 × 500")
                                                 onClicked: win.applyPreset(500, 500, true, 0, 0)
                                                 background: Rectangle { radius: 4; color: compactPreset.hovered ? win.raisedSurface : "transparent"; border.color: compactPreset.hovered ? theme.accent : win.outline }
                                             }
                                             Button {
                                                 id: pipPreset
                                                 text: qsTr("Picture-in-picture")
                                                 onClicked: win.applyPreset(420, 280, false, 24, 64)
                                                 background: Rectangle { radius: 4; color: pipPreset.hovered ? win.raisedSurface : "transparent"; border.color: pipPreset.hovered ? theme.accent : win.outline }
                                             }
                                             Button {
                                                 id: largePreset
                                                 text: qsTr("Large · 900 × 700")
                                                 onClicked: win.applyPreset(900, 700, true, 0, 0)
                                                 background: Rectangle { radius: 4; color: largePreset.hovered ? win.raisedSurface : "transparent"; border.color: largePreset.hovered ? theme.accent : win.outline }
                                             }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                     implicitHeight: sizeContent.implicitHeight + 24
                                     radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: sizeContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                         anchors.margins: 12; spacing: 12
                                        ColumnLayout {
                                            spacing: 3
                                            ModuleHeading {
                                                title: qsTr("Size & position"); helpKey: "size"
                                                explanation: qsTr("Set the floating window's width and height in logical pixels. Center places it in the middle of its monitor. Set position uses X and Y offsets from that monitor's top-left corner instead. Size and position are intended for floating windows.")
                                            }
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
                                        Label { Layout.fillWidth: true; visible: !!win.fieldErrors.width || !!win.fieldErrors.height; text: (win.fieldErrors.width || "") + "\n" + (win.fieldErrors.height || ""); color: "#ffb778"; wrapMode: Text.WordWrap }
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
                                     implicitHeight: constraintsContent.implicitHeight + 24
                                    visible: win.advancedMode
                                     radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: constraintsContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                         anchors.margins: 12; spacing: 8
                                        ModuleHeading {
                                            title: qsTr("Size constraints"); helpKey: "constraints"
                                            explanation: qsTr("Optionally limit how small or large matching windows can become. Enable Minimum or Maximum and enter width × height in logical pixels. Leave a limit unchecked to omit it from the rule. Applications may also enforce their own size limits.")
                                        }
                                        Label { Layout.fillWidth: true; visible: !!win.fieldErrors.constraints; text: win.fieldErrors.constraints || ""; color: "#ffb778"; wrapMode: Text.WordWrap }
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
                                 spacing: 18
                                Rectangle {
                                    Layout.fillWidth: true
                                     implicitHeight: placementCard.implicitHeight + 24
                                     radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: placementCard
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                         anchors.margins: 12; spacing: 14
                                        ModuleHeading {
                                            title: qsTr("Workspace & visibility"); helpKey: "workspace"
                                            explanation: qsTr("Choose the workspace where matching windows should open. Leave Workspace empty to keep the current workspace behavior. Add silent, for example 3 silent, to avoid switching to that workspace. Pin keeps a floating window visible as you change workspaces.")
                                        }
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
                                     implicitHeight: scopeCard.implicitHeight + 24
                                     radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: scopeCard
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                         anchors.margins: 12; spacing: 9
                                        ModuleHeading {
                                            title: qsTr("Rule matching"); helpKey: "matching"
                                            explanation: qsTr("Decide which windows receive this rule. Class identifies the application; matching the initial title narrows it to a particular kind of window. Exact, Contains, and Regular expression control how text is compared. Extra conditions narrow the match further. Windows with identical class and title can share the rule.")
                                        }
                        CheckBox { id: titleMatch; text: qsTr("Match this window's title too"); checked: true; onToggled: win.refreshPreview() }
                        ComboBox {
                            objectName: "scopeChoice"
                            Layout.fillWidth: true
                            model: [qsTr("This application"), qsTr("Application and title"), qsTr("Advanced conditions")]
                            currentIndex: win.advancedMode ? 2 : titleMatch.checked ? 1 : 0
                            onActivated: { if (currentIndex < 2) titleMatch.checked = currentIndex === 1; else win.advancedMode = true; win.refreshPreview() }
                        }
                        TextField { id: classPatternField; Layout.fillWidth: true; placeholderText: qsTr("Class pattern (empty uses selected application)"); Accessible.name: qsTr("Class pattern"); onTextChanged: win.refreshPreview() }
                        TextField { id: titlePatternField; Layout.fillWidth: true; enabled: titleMatch.checked; placeholderText: qsTr("Title pattern (empty uses selected title)"); Accessible.name: qsTr("Title pattern"); onTextChanged: win.refreshPreview() }
                        Label { Layout.fillWidth: true; color: "#ffb778"; visible: !!win.fieldErrors["class"] || !!win.fieldErrors.title; wrapMode: Text.WordWrap; text: (win.fieldErrors["class"] || "") + "\n" + (win.fieldErrors.title || "") }
                        RowLayout {
                            Layout.fillWidth: true
                            enabled: titleMatch.checked
                            visible: win.advancedMode
                            Label { text: qsTr("Title match"); Layout.preferredWidth: 105; font.pixelSize: 11; opacity: 0.7 }
                            ComboBox { id: titleMatchMode; model: [qsTr("Exact"), qsTr("Contains"), qsTr("Regular expression")]; Layout.fillWidth: true; onCurrentIndexChanged: win.refreshPreview() }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: win.advancedMode
                            Label { text: qsTr("Class match"); Layout.preferredWidth: 105; font.pixelSize: 11; opacity: 0.7 }
                            ComboBox { id: classMatchMode; model: [qsTr("Exact"), qsTr("Contains"), qsTr("Regular expression")]; Layout.fillWidth: true; onCurrentIndexChanged: win.refreshPreview() }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: win.advancedMode
                            enabled: !titleMatch.checked
                            Label { text: qsTr("Exclude titles (full-match regular expression)"); font.pixelSize: 11; opacity: 0.7 }
                            TextField {
                                id: excludeTitleField
                                Layout.fillWidth: true
                                placeholderText: qsTr("e.g. ^Settings$")
                                onTextChanged: win.refreshPreview()
                            }
                            Label { Layout.fillWidth: true; visible: !!win.fieldErrors.excludeTitle; text: win.fieldErrors.excludeTitle || ""; color: "#ffb778"; wrapMode: Text.WordWrap }
                            Label { text: qsTr("Hyprland's negative: selector excludes matching titles."); font.pixelSize: 10; opacity: 0.5 }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: win.outline }
                        Label { visible: win.advancedMode; text: qsTr("Additional match conditions"); font.pixelSize: 12; font.bold: true }
                        CheckBox { id: workspaceMatch; visible: win.advancedMode; text: qsTr("Only on the selected window's workspace"); onToggled: win.refreshPreview() }
                        CheckBox { id: xdgTagMatch; visible: win.advancedMode; text: qsTr("Match the XDG tag"); enabled: backend.selectedWindow.xdgTag !== ""; onToggled: win.refreshPreview() }
                        CheckBox { id: xwaylandMatch; visible: win.advancedMode; text: qsTr("Match XWayland state (on or off)"); onToggled: win.refreshPreview() }
                        CheckBox { id: fullscreenMatch; visible: win.advancedMode; text: qsTr("Match fullscreen state"); onToggled: win.refreshPreview() }
                        CheckBox { id: groupMatch; visible: win.advancedMode; text: qsTr("Match grouped state"); onToggled: win.refreshPreview() }
                        CheckBox { id: contentMatch; visible: win.advancedMode; text: qsTr("Match content type"); enabled: backend.selectedWindow.contentType !== ""; onToggled: win.refreshPreview() }
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
                                 spacing: 18
                                Rectangle {
                                    Layout.fillWidth: true
                                     implicitHeight: opacityContent.implicitHeight + 24
                                     radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: opacityContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                         anchors.margins: 12; spacing: 10
                                        RowLayout {
                                            Layout.fillWidth: true
                                            ColumnLayout {
                                                spacing: 3
                                                ModuleHeading {
                                                    title: qsTr("Opacity"); helpKey: "opacity"
                                                    explanation: qsTr("Enable to override transparency for matching windows. Focused applies to the active window; Unfocused applies when another window is active. 1.00 is fully opaque and lower values are more transparent. Disable to leave opacity out of this rule.")
                                                }
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
                                     implicitHeight: chromeContent.implicitHeight + 24
                                     radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: chromeContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                         anchors.margins: 12; spacing: 10
                                        ModuleHeading {
                                            title: qsTr("Window decoration"); helpKey: "decoration"
                                            explanation: qsTr("Override the compositor's border thickness and corner rounding for matching windows. Values are in pixels; zero removes that decoration. Each override must be enabled separately. Unchecked options keep the normal theme or other applicable rules.")
                                        }
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
                                    implicitHeight: effectsContent.implicitHeight + 24
                                    radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: effectsContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 12; spacing: 8
                                        ModuleHeading {
                                            title: qsTr("Visual effects"); helpKey: "effects"
                                            explanation: qsTr("Disable background blur or the outside window shadow for matching windows. These checkboxes turn effects off, not on. Leaving them unchecked omits the override and lets your normal Hyprland configuration decide.")
                                        }
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
                                spacing: 18
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: focusContent.implicitHeight + 24
                                    radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: focusContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 12; spacing: 8
                                        ModuleHeading {
                                            title: qsTr("Focus & interaction"); helpKey: "focus"
                                            explanation: qsTr("Prevent matching windows from taking keyboard focus automatically when they open. This is useful for passive panels and notifications. You can still focus the window yourself later; the option does not disable keyboard or mouse input.")
                                        }
                                        CheckBox { id: focusBox; text: qsTr("Don't focus this window when it opens"); onToggled: win.refreshPreview() }
                                        Label { text: qsTr("Useful for notification windows and passive panels."); font.pixelSize: 11; opacity: 0.58; wrapMode: Text.WordWrap }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: performanceContent.implicitHeight + 24
                                    radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: performanceContent
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 12; spacing: 8
                                        ModuleHeading {
                                            title: qsTr("Performance & persistence"); helpKey: "performance"
                                            explanation: qsTr("Set per-window exceptions for animations, inactive-window dimming, and variable refresh rate (VRR). Remember floating size asks Hyprland to preserve the floating size for matching windows. These settings apply to this rule's targets, not the whole desktop.")
                                        }
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
                                spacing: 18
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: savedIntro.implicitHeight + 24
                                    radius: 0; color: "transparent"; border.color: "transparent"
                                    ColumnLayout {
                                        id: savedIntro
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                        anchors.margins: 12; spacing: 5
                                        ModuleHeading {
                                            title: qsTr("Saved rules"); helpKey: "saved"
                                            explanation: qsTr("Manage rules stored in your Hyprland configuration. Enable or disable, apply to matching open windows, duplicate, edit, reorder, or remove a rule using its row actions. Overlap warns that rules may target the same windows. Import and export transfer the complete collection as JSON; review imports before confirming.")
                                        }
                                         Label {
                                             Layout.fillWidth: true
                                             text: qsTr("Manage persistent Hyprland window rules. Import and export the complete rule collection as JSON.")
                                             wrapMode: Text.WordWrap; font.pixelSize: 11; opacity: 0.6
                                         }
                                         RowLayout {
                                             Layout.fillWidth: true
                                             Button { text: qsTr("Import / organize"); onClicked: managementDialog.open() }
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
                                         implicitHeight: ruleRow.implicitHeight + 18
                                         radius: 3; color: "transparent"; border.color: win.outline
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
                    objectName: "rulePreviewPanel"
                    Layout.fillWidth: true
                    visible: win.previewExpanded
                    Layout.preferredHeight: 148
                    radius: 8
                    color: win.surface
                    border.color: win.outline
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 5
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Rule preview"); font.pixelSize: 10; font.weight: Font.Medium; color: win.mutedForeground }
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
                    objectName: "ruleActionBar"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 68
                    radius: 8
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
                            text: win.dirty ? qsTr("Unsaved draft · %1").arg(backend.status) : backend.status
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            font.pixelSize: 11
                            opacity: 0.78
                        }
                        Button {
                            objectName: "previewAction"
                            text: qsTr("Preview")
                            flat: true
                            checked: win.previewExpanded
                            checkable: true
                            onClicked: win.previewExpanded = !win.previewExpanded
                        }
                        Button {
                            id: saveAction
                            objectName: "saveRuleAction"
                            text: win.editingRuleIndex >= 0 ? qsTr("Update rule") : qsTr("Apply & save")
                            highlighted: true
                            visible: win.category < 4
                            enabled: win.editTargetAvailable && (win.editingRuleIndex >= 0 || !!backend.selectedWindow["class"]) && Object.keys(win.fieldErrors).length === 0
                            background: Rectangle { radius: 6; color: saveAction.enabled ? (saveAction.hovered ? Qt.lighter(theme.accent, 1.12) : theme.accent) : win.raisedSurface }
                            contentItem: Label { text: saveAction.text; color: saveAction.enabled ? "#081116" : win.mutedForeground; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: win.requestSave()
                        }
                        Button {
                            text: qsTr("Cancel edit")
                            visible: win.editingRuleIndex >= 0
                            onClicked: { win.editingRuleIndex = -1; win.editingRuleId = ""; win.refreshPreview() }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        classPatternField.text = backend.selectedWindow.currentClass || backend.selectedWindow["class"] || ""
        titlePatternField.text = backend.selectedWindow.currentTitle || backend.selectedWindow.title || ""
        refreshPreview(); markClean(); editorReady = true
        if (!backend.preference("onboarded", false)) Qt.callLater(function() { welcomeDialog.open() })
    }
}
