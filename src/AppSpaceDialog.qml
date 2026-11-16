import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

StudioDialog {
    id: dialog
    required property var host
    font.pixelSize: Math.round(13 * host.textScale)
    objectName: "appSpaceDialog"
    anchors.centerIn: parent
    width: Math.min(host.width - 48, 1080)
    height: Math.min(host.height - 48, 760)
    modal: true
    title: qsTr("Apps, workspaces & displays")
    standardButtons: Dialog.Close

    readonly property var workspaceChoices: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"]
    readonly property var rotationNames: [qsTr("Normal"), qsTr("90°"), qsTr("180°"), qsTr("270°"),
        qsTr("Flipped"), qsTr("Flipped 90°"), qsTr("Flipped 180°"), qsTr("Flipped 270°")]

    property string search: ""
    property string selectedClass: ""
    property string appFilter: "All"
    property string feedback: ""

    // Editor fields mirror the saved rule. They are loaded when an app is
    // chosen and only written when Apply is pressed.
    property string editorWorkspace: ""
    property bool editorSilent: false
    property bool editorAutostart: false
    property bool editorStartupOnly: false
    property int editorStartupWindow: 20

    property var importPreview: ({})
    // The catalogue scans desktop entries, so it is copied once per change
    // instead of being rebuilt for every keystroke in the search box.
    property var catalog: []

    Connections {
        target: backend
        function onCatalogChanged() { dialog.catalog = backend.applicationCatalog }
    }

    readonly property var visibleApps: {
        const rows = catalog || []
        const needle = search.trim().toLocaleLowerCase()
        const result = []
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            if (appFilter === "Running" && row.running !== true) continue
            if (appFilter === "Pinned" && row.pinned !== true) continue
            if (appFilter === "Installed" && row.running === true) continue
            if (needle) {
                const text = ((row.name || "") + " " + (row["class"] || "") + " " + (row.desktop || "")).toLocaleLowerCase()
                if (text.indexOf(needle) === -1) continue
            }
            result.push(row)
        }
        return result
    }

    readonly property var appRows: {
        const rows = visibleApps
        const result = []
        const groups = [
            { title: qsTr("OPEN NOW"), accepts: function(app) { return app.running === true } },
            { title: qsTr("YOUR RULES"), accepts: function(app) { return app.running !== true && app.pinned === true } },
            { title: qsTr("INSTALLED"), accepts: function(app) { return app.running !== true && app.pinned !== true } }
        ]
        for (let g = 0; g < groups.length; ++g) {
            const members = []
            for (let i = 0; i < rows.length; ++i)
                if (groups[g].accepts(rows[i])) members.push(rows[i])
            if (members.length === 0) continue
            result.push({ kind: "header", title: groups[g].title, count: members.length })
            for (let i = 0; i < members.length; ++i) result.push(members[i])
        }
        return result
    }

    function appInitials(app) {
        const words = (app.name || app["class"] || "?").trim().split(/[\s._-]+/)
        return words.length > 1 ? (words[0].charAt(0) + words[1].charAt(0)).toUpperCase()
                                : words[0].slice(0, 2).toUpperCase()
    }

    function placementSummary(app) {
        if (!app || !app.pinned) return qsTr("No placement rule")
        if (app.workspace) return qsTr("Workspace %1%2").arg(app.workspace)
            .arg(app.startupOnly ? qsTr(" · at login") : "")
        if (app.autostart) return qsTr("Launches at login")
        if (app.silent) return qsTr("Silent placement")
        return qsTr("Rule saved")
    }

    readonly property var selectedApp: {
        const rows = catalog || []
        for (let i = 0; i < rows.length; ++i)
            if (rows[i]["class"] === selectedClass) return rows[i]
        return null
    }

    readonly property var monitorOptions: {
        const rows = backend.monitors || []
        const result = [{ label: qsTr("Unassigned"), value: "" }]
        for (let i = 0; i < rows.length; ++i) {
            const description = rows[i].description || ""
            if (!description) continue
            result.push({ label: (rows[i].name || qsTr("Monitor")) + " · " + description, value: description })
        }
        return result
    }

    function monitorIndex(description) {
        for (let i = 0; i < monitorOptions.length; ++i)
            if (monitorOptions[i].value === (description || "")) return i
        return 0
    }

    function choose(item) {
        selectedClass = item["class"]
        editorWorkspace = item.workspace || ""
        editorSilent = item.silent === true
        editorAutostart = item.autostart === true
        editorStartupOnly = item.startupOnly === true
        editorStartupWindow = item.startupWindow || 20
    }

    function applySelected() {
        const item = selectedApp
        if (!item) return
        backend.setApplicationPlacement(item["class"], {
            name: item.name || item["class"],
            workspace: editorWorkspace,
            silent: editorSilent,
            autostart: editorAutostart,
            startupOnly: editorStartupOnly,
            startupWindow: editorStartupWindow,
            launchDesktop: item.desktop || "",
            launchCommand: item.command || ""
        })
        feedback = backend.status
    }

    function clearSelected() {
        if (!selectedApp) return
        backend.clearApplicationPlacement(selectedClass)
        feedback = backend.status
    }

    function saveWorkspace(slot, monitor, persistent) {
        backend.setWorkspacePlan(slot, monitor, persistent)
        feedback = backend.status
    }

    function clearWorkspace(slot) {
        backend.removeWorkspacePlan(slot)
        feedback = backend.status
    }

    function applyDisplay(description, layout) {
        backend.applyMonitorLayout(description, layout)
        feedback = backend.status
    }

    function previewImport() {
        importPreview = backend.appSpaceImportPreview(importPath.text)
    }

    function importRules() {
        backend.importAppSpace(importPath.text)
        feedback = backend.status
    }

    onOpened: {
        search = ""
        appFilter = "All"
        feedback = ""
        catalog = backend.applicationCatalog
        if (catalog.length > 0) choose(catalog[0])
        else selectedClass = ""
    }

    contentItem: ColumnLayout {
        spacing: 12
        TabBar {
            id: tabs
            objectName: "appSpaceTabs"
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            Layout.minimumHeight: 42
            background: Rectangle { radius: 9; color: "#111c24"; border.color: "#263642" }
            TabButton {
                text: qsTr("Apps")
                background: Rectangle {
                    radius: 7
                    color: parent.checked ? Qt.alpha(theme.accent, 0.16) : "transparent"
                    border.color: parent.checked ? Qt.alpha(theme.accent, 0.55) : "transparent"
                }
            }
            TabButton {
                text: qsTr("Workspaces")
                background: Rectangle {
                    radius: 7
                    color: parent.checked ? Qt.alpha(theme.accent, 0.16) : "transparent"
                    border.color: parent.checked ? Qt.alpha(theme.accent, 0.55) : "transparent"
                }
            }
            TabButton {
                text: qsTr("Displays")
                background: Rectangle {
                    radius: 7
                    color: parent.checked ? Qt.alpha(theme.accent, 0.16) : "transparent"
                    border.color: parent.checked ? Qt.alpha(theme.accent, 0.55) : "transparent"
                }
            }
            TabButton {
                text: qsTr("Import")
                background: Rectangle {
                    radius: 7
                    color: parent.checked ? Qt.alpha(theme.accent, 0.16) : "transparent"
                    border.color: parent.checked ? Qt.alpha(theme.accent, 0.55) : "transparent"
                }
            }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            // ------------------------------------------------------------- apps
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Apps")
                        font.pixelSize: 21
                        font.bold: true
                    }
                    Label {
                        text: qsTr("%1 apps").arg(dialog.visibleApps.length)
                        color: "#82939f"
                        font.pixelSize: 11
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    TextField {
                        id: appSearch
                        objectName: "appSpaceSearch"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search apps and windows…")
                        text: dialog.search
                        onTextChanged: dialog.search = text
                        Accessible.name: qsTr("Search applications")
                        background: Rectangle {
                            radius: 8
                            color: "#111c24"
                            border.color: appSearch.activeFocus ? theme.accent : "#2b3c48"
                        }
                    }
                    Repeater {
                        model: [qsTr("All"), qsTr("Running"), qsTr("Pinned"), qsTr("Installed")]
                        delegate: Button {
                            required property string modelData
                            checkable: true
                            checked: dialog.appFilter === modelData
                            text: modelData
                            onClicked: dialog.appFilter = modelData
                            background: Rectangle {
                                radius: 7
                                color: checked ? Qt.alpha(theme.accent, 0.18) : "#111c24"
                                border.color: checked ? Qt.alpha(theme.accent, 0.55) : "#263642"
                            }
                            contentItem: Label {
                                text: modelData
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                color: checked ? theme.accent : "#9baab4"
                                font.pixelSize: 11
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12
                    ListView {
                        id: appList
                        objectName: "appSpaceList"
                        Layout.preferredWidth: 390
                        Layout.fillHeight: true
                        clip: true
                        spacing: 3
                        model: dialog.appRows
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Item {
                            id: appRow
                            required property int index
                            required property var modelData
                            readonly property bool header: modelData.kind === "header"
                            readonly property bool selected: !header && dialog.selectedClass === modelData["class"]
                            width: ListView.view.width
                            height: header ? 30 : 64
                            Rectangle {
                                anchors.fill: parent
                                anchors.leftMargin: appRow.header ? 0 : 2
                                anchors.rightMargin: appRow.header ? 0 : 2
                                radius: 8
                                color: appRow.selected ? Qt.alpha(theme.accent, 0.13)
                                    : (rowMouse.containsMouse && !appRow.header ? "#17242d" : "transparent")
                                border.color: appRow.selected ? Qt.alpha(theme.accent, 0.55) : "transparent"
                                visible: !appRow.header
                            }
                            Label {
                                visible: appRow.header
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 3
                                text: modelData.title + "  ·  " + modelData.count
                                color: "#728490"
                                font.pixelSize: 10
                                font.bold: true
                            }
                            RowLayout {
                                visible: !appRow.header
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 10
                                spacing: 10
                                Rectangle {
                                    Layout.preferredWidth: 38
                                    Layout.preferredHeight: 38
                                    radius: 11
                                    color: Qt.alpha(theme.accent, appRow.selected ? 0.25 : 0.13)
                                    Label {
                                        anchors.centerIn: parent
                                        text: dialog.appInitials(modelData)
                                        color: theme.accent
                                        font.bold: true
                                        font.pixelSize: 12
                                        visible: !(modelData.iconData || "").length
                                    }
                                    Image {
                                        anchors.centerIn: parent
                                        width: 28
                                        height: 28
                                        source: modelData.iconData || ""
                                        visible: (modelData.iconData || "").length > 0
                                        asynchronous: true
                                        smooth: true
                                        fillMode: Image.PreserveAspectFit
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.name || modelData["class"] || ""
                                        elide: Text.ElideRight
                                        font.pixelSize: 12
                                        font.bold: appRow.selected
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: (modelData["class"] || "") + (modelData.inferred ? qsTr(" · guessed") : "")
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                        color: "#82939f"
                                    }
                                }
                                Column {
                                    spacing: 4
                                    Label {
                                        visible: modelData.running === true
                                        text: qsTr("OPEN")
                                        color: "#8bd5a5"
                                        font.pixelSize: 9
                                        font.bold: true
                                        horizontalAlignment: Text.AlignRight
                                    }
                                    Label {
                                        visible: modelData.pinned === true
                                        text: dialog.placementSummary(modelData)
                                        color: theme.accent
                                        font.pixelSize: 9
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                enabled: !appRow.header
                                hoverEnabled: true
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: dialog.choose(modelData)
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        color: "#263642"
                    }
                    Rectangle {
                        id: appDetails
                        objectName: "appSpaceDetails"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 12
                        color: "#111c24"
                        border.color: "#263642"
                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 16
                            anchors.bottomMargin: 76
                            objectName: "toolScroll"
                            contentWidth: availableWidth
                            clip: true
                            ColumnLayout {
                                width: parent.width
                                spacing: 14
                                Item {
                                    visible: dialog.selectedApp === null
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    implicitHeight: 300
                                    Column {
                                        anchors.centerIn: parent
                                        width: Math.min(parent.width, 300)
                                        spacing: 10
                                        Rectangle {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            width: 58; height: 58; radius: 18
                                            color: Qt.alpha(theme.accent, 0.13)
                                            Label { anchors.centerIn: parent; text: "⌘"; color: theme.accent; font.pixelSize: 27 }
                                        }
                                        Label {
                                            width: parent.width
                                            text: qsTr("Choose an app")
                                            horizontalAlignment: Text.AlignHCenter
                                            font.pixelSize: 18
                                            font.bold: true
                                        }
                                        Label {
                                            width: parent.width
                                            text: qsTr("Select an app to set where it opens, whether it stays quiet, or if it launches when you log in.")
                                            wrapMode: Text.WordWrap
                                            horizontalAlignment: Text.AlignHCenter
                                            color: "#82939f"
                                        }
                                    }
                                }
                                ColumnLayout {
                                    visible: dialog.selectedApp !== null
                                    Layout.fillWidth: true
                                    spacing: 14
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12
                                        Rectangle {
                                            Layout.preferredWidth: 52; Layout.preferredHeight: 52
                                            radius: 15
                                            color: Qt.alpha(theme.accent, 0.17)
                                            Label {
                                                anchors.centerIn: parent
                                                text: dialog.selectedApp ? dialog.appInitials(dialog.selectedApp) : ""
                                                color: theme.accent
                                                font.pixelSize: 16
                                                font.bold: true
                                                visible: !dialog.selectedApp || !(dialog.selectedApp.iconData || "").length
                                            }
                                            Image {
                                                anchors.centerIn: parent
                                                width: 38
                                                height: 38
                                                source: dialog.selectedApp ? (dialog.selectedApp.iconData || "") : ""
                                                visible: dialog.selectedApp && (dialog.selectedApp.iconData || "").length > 0
                                                asynchronous: true
                                                smooth: true
                                                fillMode: Image.PreserveAspectFit
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Label {
                                                Layout.fillWidth: true
                                                text: dialog.selectedApp ? (dialog.selectedApp.name || "") : ""
                                                font.pixelSize: 19
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: dialog.selectedApp ? dialog.selectedApp["class"] : ""
                                                font.pixelSize: 10
                                                color: "#82939f"
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Repeater {
                                            model: dialog.selectedApp ? [
                                                { text: dialog.selectedApp.running ? qsTr("OPEN NOW") : qsTr("NOT RUNNING"),
                                                  color: dialog.selectedApp.running ? "#8bd5a5" : "#82939f" },
                                                { text: dialog.selectedApp.verified ? qsTr("CLASS VERIFIED") : qsTr("CLASS GUESSED"),
                                                  color: dialog.selectedApp.verified ? "#8bd5a5" : "#ffbd72" },
                                                { text: dialog.placementSummary(dialog.selectedApp),
                                                  color: dialog.selectedApp.pinned ? theme.accent : "#82939f" }
                                            ] : []
                                            delegate: Rectangle {
                                                required property var modelData
                                                width: chipText.implicitWidth + 18
                                                height: 24
                                                radius: 12
                                                color: Qt.alpha(modelData.color, 0.12)
                                                Label {
                                                    id: chipText
                                                    anchors.centerIn: parent
                                                    text: modelData.text
                                                    color: modelData.color
                                                    font.pixelSize: 9
                                                    font.bold: true
                                                }
                                            }
                                        }
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 1; color: "#263642" }
                                    Label { text: qsTr("OPENS ON"); color: "#82939f"; font.pixelSize: 10; font.bold: true }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Repeater {
                                            model: [qsTr("Any")].concat(dialog.workspaceChoices)
                                            delegate: Button {
                                                required property int index
                                                required property string modelData
                                                objectName: index === 0 ? "appSpaceWorkspace" : ""
                                                checkable: true
                                                checked: (index === 0 && dialog.editorWorkspace === "")
                                                    || (index > 0 && dialog.editorWorkspace === modelData)
                                                text: modelData
                                                onClicked: dialog.editorWorkspace = index === 0 ? "" : modelData
                                                background: Rectangle {
                                                    radius: 7
                                                    color: checked ? Qt.alpha(theme.accent, 0.2) : "#17242d"
                                                    border.color: checked ? Qt.alpha(theme.accent, 0.65) : "#2b3c48"
                                                }
                                                contentItem: Label {
                                                    text: modelData
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                    color: checked ? theme.accent : "#c2cdd6"
                                                    font.pixelSize: 11
                                                }
                                            }
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Workspace pins follow the display assignment in the Workspaces tab.")
                                        wrapMode: Text.WordWrap
                                        color: "#82939f"
                                        font.pixelSize: 10
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 1; color: "#263642" }
                                    CheckBox {
                                        objectName: "appSpaceSilent"
                                        text: qsTr("Open quietly")
                                        checked: dialog.editorSilent
                                        onToggled: dialog.editorSilent = checked
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Keep focus on what you are doing when this app opens.")
                                        color: "#82939f"
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                    CheckBox {
                                        objectName: "appSpaceAutostart"
                                        text: qsTr("Launch when I log in")
                                        enabled: dialog.selectedApp !== null &&
                                            ((dialog.selectedApp.desktop || "") !== "" || (dialog.selectedApp.command || "") !== "")
                                        checked: dialog.editorAutostart
                                        onToggled: dialog.editorAutostart = checked
                                    }
                                    CheckBox {
                                        objectName: "appSpaceStartupOnly"
                                        text: qsTr("Only place windows opened at login")
                                        enabled: dialog.editorWorkspace !== ""
                                        checked: dialog.editorStartupOnly
                                        onToggled: dialog.editorStartupOnly = checked
                                    }
                                    RowLayout {
                                        visible: dialog.editorStartupOnly
                                        Label { Layout.fillWidth: true; text: qsTr("Login window"); color: "#82939f" }
                                        SpinBox {
                                            objectName: "appSpaceStartupWindow"
                                            from: 1; to: 3600; editable: true
                                            value: dialog.editorStartupWindow
                                            onValueModified: dialog.editorStartupWindow = value
                                        }
                                        Label { text: qsTr("seconds"); color: "#82939f" }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Changes are saved to WindowTuner and applied to Hyprland. AppSpace's files are never changed.")
                                        wrapMode: Text.WordWrap
                                        color: "#71828e"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 64
                            color: "#111c24"
                            border.width: 0
                            Rectangle {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 1
                                color: "#263642"
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 8
                                Label {
                                    Layout.fillWidth: true
                                    visible: dialog.feedback.length > 0
                                    text: dialog.feedback
                                    color: "#82939f"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                                Button {
                                    objectName: "appSpaceClear"
                                    text: qsTr("Remove rule")
                                    enabled: dialog.selectedApp !== null && dialog.selectedApp.pinned === true
                                    onClicked: dialog.clearSelected()
                                }
                                Button {
                                    objectName: "appSpaceApply"
                                    text: qsTr("Save placement")
                                    enabled: dialog.selectedApp !== null
                                    onClicked: dialog.applySelected()
                                    background: Rectangle {
                                        radius: 8
                                        color: theme.accent
                                    }
                                    contentItem: Label {
                                        text: qsTr("Save placement")
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        color: "#111820"
                                        font.bold: true
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------------- workspaces
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#93a4b3"
                    font.pixelSize: 11
                    text: qsTr("Pin a workspace to a display so it always lives there, and mark it always present. Assigned apps are listed per workspace.")
                }
                ListView {
                    id: workspaceList
                    objectName: "appSpaceWorkspaceList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: backend.workspacePlan
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        implicitHeight: row.implicitHeight + 16
                        radius: 6
                        color: "#15212a"
                        border.color: "#24313b"
                        RowLayout {
                            id: row
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8
                            Label {
                                text: qsTr("Workspace %1").arg(modelData.slot)
                                Layout.preferredWidth: 110
                                font.bold: true
                            }
                            ComboBox {
                                id: workspaceMonitor
                                Layout.preferredWidth: 260
                                model: dialog.monitorOptions
                                textRole: "label"
                                currentIndex: dialog.monitorIndex(modelData.monitor)
                            }
                            CheckBox {
                                id: workspacePersistent
                                text: qsTr("Always present")
                                checked: modelData.persistent === true
                            }
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                color: "#93a4b3"
                                font.pixelSize: 10
                                text: (modelData.apps || []).length > 0
                                    ? (modelData.apps || []).join(", ")
                                    : qsTr("No apps assigned")
                            }
                            Button {
                                text: qsTr("Save")
                                onClicked: dialog.saveWorkspace(modelData.slot,
                                    dialog.monitorOptions[workspaceMonitor.currentIndex].value,
                                    workspacePersistent.checked)
                            }
                            Button {
                                text: qsTr("Clear")
                                enabled: (modelData.monitor || "") !== ""
                                onClicked: dialog.clearWorkspace(modelData.slot)
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: dialog.feedback.length > 0
                    text: dialog.feedback
                    color: "#c2cdd6"
                }
            }

            // --------------------------------------------------------- displays
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#93a4b3"
                        font.pixelSize: 11
                        text: qsTr("Saved by make, model and serial, so a display keeps its layout on another port. A new layout reverts after 15 seconds unless you keep it; the countdown runs in this app, so keep WindowTuner open until you decide.")
                    }
                    Button {
                        objectName: "appSpaceKeep"
                        visible: backend.monitorRevertPending
                        text: qsTr("Keep layout")
                        onClicked: { backend.keepMonitorLayout(); dialog.feedback = backend.status }
                    }
                    Button {
                        objectName: "appSpaceRevert"
                        visible: backend.monitorRevertPending
                        text: qsTr("Revert now")
                        onClicked: { backend.revertMonitorLayout(); dialog.feedback = backend.status }
                    }
                }
                ListView {
                    id: monitorList
                    objectName: "appSpaceMonitorList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: backend.monitorPlan
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        implicitHeight: monitorRow.implicitHeight + 16
                        radius: 6
                        color: "#15212a"
                        border.color: "#24313b"
                        GridLayout {
                            id: monitorRow
                            anchors.fill: parent
                            anchors.margins: 8
                            columns: 6
                            columnSpacing: 8
                            rowSpacing: 6
                            Label {
                                Layout.columnSpan: 6
                                Layout.fillWidth: true
                                text: modelData.name + "  ·  " + modelData.description
                                    + (modelData.saved ? qsTr("  · saved") : "")
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.columnSpan: 6
                                Layout.fillWidth: true
                                visible: (modelData.description || "") === ""
                                wrapMode: Text.WordWrap
                                color: "#ffb778"
                                font.pixelSize: 10
                                text: qsTr("This display reports no make, model or serial, so its layout cannot be saved by identity.")
                            }
                            Label { text: qsTr("Mode"); color: "#93a4b3"; font.pixelSize: 10 }
                            ComboBox {
                                id: modeBox
                                Layout.preferredWidth: 230
                                model: {
                                    const list = (modelData.modes || []).slice()
                                    if (list.indexOf(modelData.mode) === -1) list.unshift(modelData.mode)
                                    return list
                                }
                                Component.onCompleted: {
                                    const at = model.indexOf(modelData.mode)
                                    currentIndex = at >= 0 ? at : 0
                                }
                            }
                            Label { text: qsTr("Rotation"); color: "#93a4b3"; font.pixelSize: 10 }
                            ComboBox {
                                id: rotationBox
                                Layout.preferredWidth: 150
                                model: dialog.rotationNames
                                currentIndex: Math.max(0, Math.min(7, modelData.transform))
                            }
                            Label { text: qsTr("Scale"); color: "#93a4b3"; font.pixelSize: 10 }
                            SpinBox {
                                id: scaleBox
                                from: 25; to: 800; stepSize: 25
                                editable: true
                                value: Math.round((modelData.scale || 1) * 100)
                                textFromValue: function(v, locale) { return (v / 100).toFixed(2) }
                                valueFromText: function(text, locale) { return Math.round(parseFloat(text.replace(",", ".")) * 100) || 100 }
                            }
                            Label { text: qsTr("X"); color: "#93a4b3"; font.pixelSize: 10 }
                            SpinBox {
                                id: xBox
                                from: -100000; to: 100000; stepSize: 10
                                editable: true
                                value: modelData.x
                            }
                            Label { text: qsTr("Y"); color: "#93a4b3"; font.pixelSize: 10 }
                            SpinBox {
                                id: yBox
                                from: -100000; to: 100000; stepSize: 10
                                editable: true
                                value: modelData.y
                            }
                            CheckBox {
                                id: enabledBox
                                Layout.columnSpan: 2
                                text: qsTr("Enabled")
                                checked: true
                            }
                            Button {
                                Layout.columnSpan: 2
                                text: qsTr("Apply layout")
                                enabled: (modelData.description || "") !== ""
                                onClicked: dialog.applyDisplay(modelData.description, {
                                    mode: modeBox.currentText,
                                    x: xBox.value,
                                    y: yBox.value,
                                    scale: scaleBox.value / 100,
                                    transform: rotationBox.currentIndex,
                                    enabled: enabledBox.checked
                                })
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: dialog.feedback.length > 0
                    text: dialog.feedback
                    color: "#c2cdd6"
                }
            }

            // ----------------------------------------------------------- import
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ScrollBar.vertical: ScrollBar {}
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: qsTr("Import an AppSpace rules file")
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#93a4b3"
                        font.pixelSize: 11
                        text: qsTr("Rules, workspace pins, aliases and display layouts are copied into WindowTuner's files. AppSpace's own generated module is not touched.")
                    }
                    TextField {
                        id: importPath
                        objectName: "appSpaceImportPath"
                        Layout.fillWidth: true
                        placeholderText: backend.appSpacePath
                        Accessible.name: qsTr("AppSpace rules file")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            objectName: "appSpaceImportPreview"
                            text: qsTr("Preview")
                            onClicked: dialog.previewImport()
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                            color: "#93a4b3"
                            font.pixelSize: 10
                            text: dialog.importPreview.path || backend.appSpacePath
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: dialog.importPreview.error
                            || (dialog.importPreview.ok
                                ? qsTr("%1 new rule(s), %2 updated, %3 workspace pin(s), %4 display(s). %5")
                                    .arg(dialog.importPreview.added).arg(dialog.importPreview.updated)
                                    .arg(dialog.importPreview.workspaces).arg(dialog.importPreview.monitors)
                                    .arg(dialog.importPreview.note || "")
                                : qsTr("Nothing has been read yet."))
                    }
                    Repeater {
                        model: dialog.importPreview.sample || []
                        delegate: Label {
                            required property var modelData
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            font.pixelSize: 10
                            color: "#93a4b3"
                            text: "• " + modelData["class"] + " → "
                                + (modelData.workspace === "" ? qsTr("any workspace") : qsTr("workspace %1").arg(modelData.workspace))
                        }
                    }
                    Button {
                        objectName: "appSpaceImportAction"
                        text: qsTr("Import into WindowTuner")
                        enabled: dialog.importPreview.ok === true
                        onClicked: dialog.importRules()
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#ffb778"
                        text: qsTr("After a successful import, turn AppSpace off or remove its generated module. Two tools placing the same app would race on reload.")
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        visible: dialog.feedback.length > 0
                        text: dialog.feedback
                        color: "#c2cdd6"
                    }
                }
            }
        }
    }
}
