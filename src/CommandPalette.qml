import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StudioDialog {
    id: commandPalette
    objectName: "commandPalette"
    required property var host
    readonly property color mutedForeground: host ? host.mutedForeground : "#9aa6ad"
    readonly property color raisedSurface: host ? host.raisedSurface : "#182127"
    property string query: ""
    property var pinned: []
    property var pendingCommand: ({})
    property string selectedCommandId: ""
    property string lastAction: ""
    property string feedback: ""
    readonly property var allCommands: {
        const inspected = backend.inspectWindow()
        const match = (inspected.rules || []).find(rule => !rule.unknown) || ({})
        const windows = backend.windows.length
        return [
            { id: "go.windows", group: qsTr("Go"), title: qsTr("Windows"), detail: qsTr("Show open windows"), run: function() { host.navigate(-1) } },
            { id: "go.desktops", group: qsTr("Go"), title: qsTr("Desktops"), detail: qsTr("Show the desktop overview"), run: function() { host.navigate(-2) } },
            { id: "go.layout", group: qsTr("Go"), title: qsTr("Layout and size"), detail: qsTr("Edit layout, dimensions, and presets"), run: function() { host.navigate(0) } },
            { id: "go.placement", group: qsTr("Go"), title: qsTr("Placement"), detail: qsTr("Choose workspace, visibility, and matching"), run: function() { host.navigate(1) } },
            { id: "go.appearance", group: qsTr("Go"), title: qsTr("Appearance"), detail: qsTr("Tune opacity, decoration, and effects"), run: function() { host.navigate(2) } },
            { id: "go.behavior", group: qsTr("Go"), title: qsTr("Behavior"), detail: qsTr("Set focus, animation, and performance"), run: function() { host.navigate(3) } },
            { id: "go.saved", group: qsTr("Go"), title: qsTr("Saved rules"), detail: qsTr("Organize persistent window rules"), run: function() { host.navigate(4) } },
            { id: "window.refresh", group: qsTr("Windows"), title: qsTr("Refresh open windows"), detail: qsTr("Re-read Hyprland clients · Ctrl+R"), run: function() { backend.refreshWindows() } },
            { id: "window.next", group: qsTr("Windows"), title: qsTr("Select next window"), detail: windows ? qsTr("%1 open window(s)").arg(windows) : qsTr("No open windows"), enabled: windows > 0,
              run: function() { host.selectNextWindow() } },
            { id: "draft.save", group: qsTr("Draft"), title: qsTr("Review and save draft"), detail: qsTr("Opens the normal change review · Ctrl+S"), enabled: !host.dirty || host.editingRuleIndex >= 0, run: function() { host.requestSave() } },
            { id: "profile.switch", group: qsTr("Workflows"), title: qsTr("Switch context profile"), detail: qsTr("Review and apply a saved setup · Ctrl+Shift+P"), enabled: !host.dirty, run: function() { host.openTool("profileSwitcher") } },
            { id: "tool.safety", group: qsTr("Tools"), title: qsTr("Safety and history"), detail: qsTr("Undo, snapshots, and trial changes"), run: function() { host.openTool("safety") } },
            { id: "tool.placement", group: qsTr("Tools"), title: qsTr("Placement designer"), detail: qsTr("Arrange a window visually"), run: function() { host.openTool("placement") } },
            { id: "tool.inspect", group: qsTr("Tools"), title: qsTr("Inspect match"), detail: qsTr("See which rules match a window"), run: function() { host.openTool("inspect") } },
            { id: "tool.discoverClass", group: qsTr("Tools"), title: qsTr("Discover app class"), detail: qsTr("Compare a live window with app hints"), run: function() { host.openTool("discoverClass") } },
            { id: "tool.describeRule", group: qsTr("Tools"), title: qsTr("Describe a rule"), detail: qsTr("Turn a short English request into a draft"), run: function() { host.openTool("describeRule") } },
            { id: "tool.workflows", group: qsTr("Tools"), title: qsTr("Presets and profiles"), detail: qsTr("Save and switch rule collections"), run: function() { host.openTool("workflows") } },
            { id: "tool.scenes", group: qsTr("Tools"), title: qsTr("Scenes and restore rehearsal"), detail: qsTr("Preview window-by-window restores"), run: function() { host.openTool("scenes") } },
            { id: "tool.migration", group: qsTr("Tools"), title: qsTr("Encrypted transfer"), detail: qsTr("Preview a portable settings bundle"), run: function() { host.openTool("migration") } },
            { id: "tool.management", group: qsTr("Tools"), title: qsTr("Applications and imports"), detail: qsTr("Create targets and organize rules"), run: function() { host.openTool("management") } },
            { id: "tool.appspace", group: qsTr("Tools"), title: qsTr("Apps, workspaces, displays"), detail: qsTr("Choose where apps open"), run: function() { host.openTool("appspace") } },
            { id: "tool.health", group: qsTr("Tools"), title: qsTr("Compatibility and health"), detail: qsTr("Check supported Hyprland features"), run: function() { host.openTool("health") } },
            { id: "tool.welcome", group: qsTr("Help"), title: qsTr("Getting started"), detail: qsTr("Open the guided walkthrough"), run: function() { host.openTool("welcome") } },
            { id: "tool.shortcuts", group: qsTr("Help"), title: qsTr("Keyboard shortcuts"), detail: qsTr("View available key commands · ?"), run: function() { host.openTool("shortcuts") } },
            { id: "history.undo", group: qsTr("Changes"), title: qsTr("Undo configuration change"), detail: backend.undoSummary ? qsTr("Restore the configuration from before “%1”").arg(backend.undoSummary) : qsTr("No earlier configuration saved"), enabled: backend.canUndo && !host.dirty, changes: true,
              summary: qsTr("Restore the saved configuration to the state before “%1”. Rules and workspace data are replaced; profile and scene records are restored when that change included them. Hyprland reloads. No apps are launched.").arg(backend.undoSummary),
              run: function() { backend.undo() } },
            { id: "history.redo", group: qsTr("Changes"), title: qsTr("Redo configuration change"), detail: backend.redoSummary ? qsTr("Reapply “%1”").arg(backend.redoSummary) : qsTr("No undone configuration"), enabled: backend.canRedo && !host.dirty, changes: true,
              summary: qsTr("Reapply the configuration change “%1”. Rules and workspace data may change; profile and scene records change when that change included them. Hyprland reloads. No apps are launched.").arg(backend.redoSummary),
              run: function() { backend.redo() } },
            { id: "window.applyRule", group: qsTr("Changes"), title: qsTr("Apply matching rule to open windows"), detail: match.name ? qsTr("Live-apply “%1”").arg(match.name) : qsTr("No enabled rule matches the selected window"), enabled: !!match.name, changes: true,
              summary: qsTr("Apply “%1” live to every open window it matches. Window size, position or appearance can change immediately. Saved rules are not changed.").arg(match.name || ""),
              run: function() { backend.applySavedRule(match.index) } }
        ]
    }
    readonly property var filteredCommands: {
        const needle = query.trim().toLocaleLowerCase()
        let commands = allCommands.filter(command => command.enabled !== false)
        if (needle.length)
            commands = commands.filter(command =>
                (command.title + " " + command.detail + " " + command.group).toLocaleLowerCase().indexOf(needle) >= 0)
        const rank = command => {
            const at = pinned.indexOf(command.id)
            return at >= 0 ? at : 1000 + allCommands.findIndex(item => item.id === command.id)
        }
        commands.sort((left, right) => rank(left) - rank(right))
        return commands.map(command => Object.assign({}, command, {
            _section: pinned.indexOf(command.id) >= 0 ? qsTr("Pinned") : command.group,
            pinned: pinned.indexOf(command.id) >= 0
        }))
    }

    title: qsTr("Command palette")
    width: Math.min(host.width - 48, 760)
    height: Math.min(host.height - 48, 680)
    modal: true
    standardButtons: Dialog.Close
    onOpened: {
        loadPins()
        query = ""
        pendingCommand = ({})
        feedback = ""
        Qt.callLater(function() {
            search.forceActiveFocus()
            commandPalette.selectCommand(list.count ? 0 : -1)
        })
    }
    onClosed: {
        query = ""
        pendingCommand = ({})
        feedback = ""
    }

    function loadPins() {
        const stored = backend.preference("pinnedCommands", [])
        const next = []
        for (let i = 0; i < stored.length && next.length < 24; ++i) {
            const id = String(stored[i])
            if (id.length && next.indexOf(id) < 0) next.push(id)
        }
        pinned = next
    }
    function togglePin(id) {
        const next = pinned.slice()
        const at = next.indexOf(id)
        if (at >= 0) next.splice(at, 1)
        else if (next.length < 24) next.push(id)
        if (!backend.setPreference("pinnedCommands", next)) {
            feedback = backend.status
            return false
        }
        pinned = next
        feedback = at >= 0 ? qsTr("Unpinned %1.").arg(id) : qsTr("Pinned %1 to the top.").arg(id)
        return true
    }
    function movePinned(id, delta) {
        const next = pinned.slice()
        const at = next.indexOf(id), destination = at + delta
        if (at < 0 || destination < 0 || destination >= next.length) return false
        next.splice(at, 1)
        next.splice(destination, 0, id)
        if (!backend.setPreference("pinnedCommands", next)) {
            feedback = backend.status
            return false
        }
        pinned = next
        return true
    }
    function activateCommand(id) {
        const command = allCommands.find(item => item.id === id)
        if (!command || command.enabled === false) return false
        if (command.changes) {
            pendingCommand = command
            confirmation.open()
            return true
        }
        execute(command)
        return true
    }
    function activateCurrent() {
        if (list.currentIndex < 0 || list.currentIndex >= filteredCommands.length) return false
        return activateCommand(filteredCommands[list.currentIndex].id)
    }
    function handleKey(event) {
        if (event.key === Qt.Key_Down) {
            moveSelection(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            moveSelection(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            activateCurrent()
            event.accepted = true
        }
    }
    function moveSelection(delta) {
        if (!list.count) return
        selectCommand(Math.max(0, Math.min(list.currentIndex + delta, list.count - 1)))
    }
    function selectCommand(index) {
        list.currentIndex = index
        selectedCommandId = index >= 0 && index < filteredCommands.length
            ? filteredCommands[index].id : ""
    }
    function restoreSelection() {
        const index = filteredCommands.findIndex(command => command.id === selectedCommandId)
        if (index >= 0) list.currentIndex = index
        else selectCommand(list.count ? 0 : -1)
    }
    function execute(command) {
        const id = command.id || ""
        pendingCommand = ({})
        close()
        lastAction = id
        if (command.run) command.run()
    }

    contentItem: ColumnLayout {
        spacing: 9
        Keys.onPressed: function(event) { commandPalette.handleKey(event) }
        Shortcut { sequence: "Down"; context: Qt.WindowShortcut; enabled: commandPalette.opened && !confirmation.opened; onActivated: commandPalette.moveSelection(1) }
        Shortcut { sequence: "Up"; context: Qt.WindowShortcut; enabled: commandPalette.opened && !confirmation.opened; onActivated: commandPalette.moveSelection(-1) }
        Shortcut { sequence: "Return"; context: Qt.WindowShortcut; enabled: commandPalette.opened && !confirmation.opened; onActivated: commandPalette.activateCurrent() }
        Shortcut { sequence: "Enter"; context: Qt.WindowShortcut; enabled: commandPalette.opened && !confirmation.opened; onActivated: commandPalette.activateCurrent() }
        TextField {
            id: search
            objectName: "commandPaletteSearch"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            placeholderText: qsTr("Type a command, tool, or section…")
            Accessible.name: qsTr("Search commands")
            selectByMouse: true
            onTextChanged: {
                commandPalette.query = text
                commandPalette.selectCommand(list.count ? 0 : -1)
            }
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Down) { commandPalette.moveSelection(1); event.accepted = true }
                else if (event.key === Qt.Key_Up) { commandPalette.moveSelection(-1); event.accepted = true }
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { commandPalette.activateCurrent(); event.accepted = true }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: commandPalette.mutedForeground
            font.pixelSize: 11
            text: qsTr("↑ ↓ choose · Enter run · ☆ pin · Ctrl+Shift+K opens this palette")
        }
        ListView {
            id: list
            objectName: "commandPaletteList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: commandPalette.filteredCommands
            // Selection is imperative so a background window refresh can replace
            // the model without stealing the user's current row. Filter changes
            // reset it from the search handler, and count changes clamp it.
            onCountChanged: {
                if (count === 0) currentIndex = -1
                else if (currentIndex < 0) currentIndex = 0
                else if (currentIndex >= count) currentIndex = count - 1
            }
            onModelChanged: Qt.callLater(commandPalette.restoreSelection)
            spacing: 2
            section.property: "_section"
            section.delegate: Label {
                required property string section
                width: ListView.view ? ListView.view.width : 0
                height: 24
                topPadding: 6
                text: section
                color: commandPalette.mutedForeground
                font.pixelSize: 10
                font.weight: Font.Medium
            }
            delegate: ItemDelegate {
                id: row
                required property var modelData
                required property int index
                width: ListView.view ? ListView.view.width : 0
                height: 54
                highlighted: ListView.isCurrentItem
                Accessible.name: modelData.title
                Accessible.description: modelData.detail
                onClicked: { commandPalette.selectCommand(index); commandPalette.activateCommand(modelData.id) }
                background: Rectangle {
                    radius: 5
                    color: row.highlighted ? Qt.alpha(theme.accent, 0.14) : row.hovered ? commandPalette.raisedSurface : "transparent"
                    border.color: row.highlighted ? Qt.alpha(theme.accent, 0.5) : "transparent"
                }
                contentItem: RowLayout {
                    spacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.title
                            color: "#e6e9eb"
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.detail
                            color: commandPalette.mutedForeground
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                    ToolButton {
                        objectName: "commandPin_" + row.modelData.id
                        text: row.modelData.pinned ? "★" : "☆"
                        Accessible.name: row.modelData.pinned ? qsTr("Unpin %1").arg(row.modelData.title) : qsTr("Pin %1").arg(row.modelData.title)
                        ToolTip.visible: hovered
                        ToolTip.text: row.modelData.pinned ? qsTr("Unpin %1").arg(row.modelData.title) : qsTr("Pin %1").arg(row.modelData.title)
                        onClicked: commandPalette.togglePin(row.modelData.id)
                    }
                    ColumnLayout {
                        visible: row.modelData.pinned
                        spacing: 0
                        ToolButton {
                            text: "↑"
                            Layout.preferredWidth: 24; Layout.preferredHeight: 22
                            Accessible.name: qsTr("Move %1 up").arg(row.modelData.title)
                            onClicked: commandPalette.movePinned(row.modelData.id, -1)
                        }
                        ToolButton {
                            text: "↓"
                            Layout.preferredWidth: 24; Layout.preferredHeight: 22
                            Accessible.name: qsTr("Move %1 down").arg(row.modelData.title)
                            onClicked: commandPalette.movePinned(row.modelData.id, 1)
                        }
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                width: parent.width - 24
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("No commands match “%1”.").arg(commandPalette.query)
                color: commandPalette.mutedForeground
            }
        }
        Label {
            objectName: "commandPaletteFeedback"
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            color: commandPalette.mutedForeground
            font.pixelSize: 11
            text: commandPalette.feedback
        }
    }

    StudioDialog {
        id: confirmation
        objectName: "commandPaletteConfirmation"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(commandPalette.width - 12, 500)
        title: qsTr("Confirm %1").arg(commandPalette.pendingCommand.title || qsTr("change"))
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        contentItem: ColumnLayout {
            spacing: 8
            Label {
                objectName: "commandPaletteConfirmSummary"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: commandPalette.pendingCommand.summary || commandPalette.pendingCommand.detail || ""
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: commandPalette.mutedForeground
                font.pixelSize: 11
                text: qsTr("Nothing changes until you choose OK.")
            }
        }
        onAccepted: commandPalette.execute(commandPalette.pendingCommand)
        onRejected: commandPalette.pendingCommand = ({})
    }
}
