# Graph Report - windowtuner  (2026-11-16)

## Corpus Check
- 31 files · ~28,567 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 17 file(s) not represented in the graph (top: .qml 10, .pro 3, (none) 2)

## Summary
- 515 nodes · 1153 edges · 33 communities (17 shown, 16 thin omitted)
- Extraction: 73% EXTRACTED · 27% INFERRED · 0% AMBIGUOUS · INFERRED: 310 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `a94fe0f8`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Backend
- backend.cpp
- safety.cpp
- windowtuner_tests.cpp
- health.cpp
- workflows.cpp
- WindowTuner one-stop implementation report
- src/monitor_recovery.lua
- QString
- tui.tsx
- WindowTunerTests
- .importsAppSpacePlacementDisplaysAndAliases
- WindowTuner implementation checklist
- appspace_runtime.lua
- cli_smoke.py
- appspace.lua
- install
- one-stop-roadmap.md
- with
- appspace.cpp
- QVariantList
- Theme
- qjsondocument
- .healthAndDiagnosticsDoNotLeakPrivateData
- placement.cpp
- qfile

## God Nodes (most connected - your core abstractions)
1. `Backend` - 154 edges
2. `WindowTunerTests` - 32 edges
3. `QVariantMap` - 31 edges
4. `QVariantList` - 20 edges
5. `Theme` - 13 edges
6. `WindowTuner one-stop implementation report` - 13 edges
7. `Backend::restoreSnapshot()` - 12 edges
8. `WindowTuner` - 11 edges
9. `WindowTuner implementation checklist` - 11 edges
10. `Backend::Backend()` - 10 edges

## Surprising Connections (you probably didn't know these)
- `main()` --references--> `Backend`  [INFERRED]
  src/main.cpp → src/backend.h
- `Backend::matchWindow()` --references--> `QVariantMap`  [EXTRACTED]
  src/inspector.cpp → src/backend.h
- `Backend::externalRuleHints()` --references--> `QVariantList`  [EXTRACTED]
  src/inspector.cpp → src/backend.h
- `Backend::profiles()` --references--> `QVariantList`  [EXTRACTED]
  src/workflows.cpp → src/backend.h
- `Backend::applicationCatalog()` --references--> `QVariantList`  [EXTRACTED]
  src/appspace.cpp → src/backend.h

## Import Cycles
- None detected.

## Communities (33 total, 16 thin omitted)

### Community 0 - "Backend"
Cohesion: 0.04
Nodes (27): Backend, m_backups, m_configDirectory, m_diskFingerprint, m_future, m_hasRefreshed, m_hyprctl, m_hyprlandConfig (+19 more)

### Community 1 - "backend.cpp"
Cohesion: 0.11
Nodes (20): Backend::commitPlacementData(), Backend::setApplicationPlacement(), Backend::Backend(), Backend::loadRuleForEditing(), QVariantMap, luaString(), luaValue(), number() (+12 more)

### Community 2 - "safety.cpp"
Cohesion: 0.10
Nodes (18): QStringList, atomic(), Backend::~Backend(), Backend::createBackup(), Backend::fingerprint(), Backend::redo(), Backend::restoreBackup(), Backend::restoreSnapshot() (+10 more)

### Community 4 - "health.cpp"
Cohesion: 0.33
Nodes (4): Backend::configureEntryPoint(), Backend::diagnosticReport(), Backend::healthReport(), query()

### Community 5 - "workflows.cpp"
Cohesion: 0.17
Nodes (11): Backend::loadLibrary(), Backend::preference(), Backend::presetSettings(), Backend::profilePreview(), Backend::profiles(), Backend::removeLibraryItem(), Backend::savePreset(), Backend::saveProfile() (+3 more)

### Community 6 - "WindowTuner one-stop implementation report"
Cohesion: 0.06
Nodes (30): 1. Safety and recovery, 2. Visual placement, 3. Inspector and match testing, 4. Presets and profiles, 5. Applications and rule management, 6. Compatibility and health, 7. Beginner experience, 8. Apps, workspaces and displays (AppSpace parity) (+22 more)

### Community 9 - "src/monitor_recovery.lua"
Cohesion: 0.23
Nodes (11): clamp(), debounce(), dispatch(), geometry(), number(), place(), reconcileNow(), schedule() (+3 more)

### Community 10 - "QString"
Cohesion: 0.11
Nodes (21): Backend::applicationCatalog(), Backend::applicationSettings(), Backend::applyMonitorLayout(), Backend::appSpaceImportPreview(), Backend::appSpaceModuleConfig(), Backend::appSpacePath(), Backend::deriveApplicationClass(), Backend::desktopExecCommand() (+13 more)

### Community 11 - "tui.tsx"
Cohesion: 0.22
Nodes (4): readTodo(), setup(), TodoRow, TodoSidebar()

### Community 14 - "WindowTuner implementation checklist"
Cohesion: 0.17
Nodes (11): Appearance and behavior, Apps, workspaces and displays (AppSpace parity), Interface polish, Layout and placement, Matching and scope, OpenCode progress tracking, Remaining boundaries, Safety, validation, and polish (+3 more)

### Community 15 - "appspace_runtime.lua"
Cohesion: 0.38
Nodes (5): block_of(), move_to(), owner(), plain(), slot_id()

### Community 17 - "appspace.lua"
Cohesion: 0.36
Nodes (5): environment(), monitor(), roots(), tempdir(), writefile()

### Community 22 - "with"
Cohesion: 0.05
Nodes (36): name, runs-on, steps, APPIMAGE_EXTRACT_AND_RUN, LINUXDEPLOY, RELEASE_TAG, TMPDIR, tag (+28 more)

### Community 23 - "appspace.cpp"
Cohesion: 0.14
Nodes (8): Backend::keepMonitorLayout(), Backend::monitorData(), Backend::workspaceData(), isPlacementOnly(), Backend::decodeRules(), Backend::exportSelected(), Backend::previewImport(), Backend::selectOfflineTarget()

### Community 24 - "QVariantList"
Cohesion: 0.25
Nodes (8): Backend::monitorPlan(), Backend::workspacePlan(), QVariantList, Backend::auditCapabilities(), Backend::installedApplications(), Backend::backups(), Backend::history(), Backend::presets()

### Community 26 - "Theme"
Cohesion: 0.20
Nodes (5): Theme, m_accent, m_debounce, m_directory, m_watcher

### Community 28 - "qjsondocument"
Cohesion: 0.29
Nodes (4): Backend::externalRuleHints(), Backend::inspectWindow(), Backend::matchWindow(), effects()

## Knowledge Gaps
- **31 isolated node(s):** `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl`, `m_rulePath`, `m_windows` (+26 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 165 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **16 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Backend` connect `Backend` to `backend.cpp`, `safety.cpp`, `health.cpp`, `workflows.cpp`, `.savesAndLoadsPersistentRule`, `QString`, `WindowTunerTests`, `.importsAppSpacePlacementDisplaysAndAliases`, `.monitorLayoutKeepsOrReverts`, `QVariantList`, `backend.h`, `.managesOfflineRulesMetadataAndSelectiveImports`, `qjsondocument`, `.healthAndDiagnosticsDoNotLeakPrivateData`, `placement.cpp`?**
  _High betweenness centrality (0.277) - this node is a cross-community bridge._
- **Are the 11 inferred relationships involving `Backend` (e.g. with `main()` and `.auditsInstalledFeatureCapabilities()`) actually correct?**
  _`Backend` has 11 INFERRED edges - model-reasoned connections that need verification._
- **What connects `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl` to the rest of the system?**
  _31 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Backend` be split into smaller, more focused modules?**
  _Cohesion score 0.044444444444444446 - nodes in this community are weakly interconnected._
- **Why does `QVariantMap` connect `backend.cpp` to `Backend`, `safety.cpp`, `health.cpp`, `workflows.cpp`, `QString`, `appspace.cpp`, `backend.h`, `qjsondocument`, `placement.cpp`?**
  _High betweenness centrality (0.030) - this node is a cross-community bridge._
- **Should `backend.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.10714285714285714 - nodes in this community are weakly interconnected._
- **Why does `WindowTunerTests` connect `WindowTunerTests` to `backend.cpp`, `safety.cpp`, `windowtuner_tests.cpp`, `.savesAndLoadsPersistentRule`, `QString`, `.importsAppSpacePlacementDisplaysAndAliases`, `.monitorLayoutKeepsOrReverts`, `.managesOfflineRulesMetadataAndSelectiveImports`, `.healthAndDiagnosticsDoNotLeakPrivateData`, `placement.cpp`?**
  _High betweenness centrality (0.029) - this node is a cross-community bridge._