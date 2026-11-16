# Graph Report - windowtuner  (2026-11-16)

## Corpus Check
- 29 files · ~29,081 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 17 file(s) not represented in the graph (top: .qml 10, .pro 3, (none) 2)

## Summary
- 476 nodes · 1116 edges · 30 communities (17 shown, 13 thin omitted)
- Extraction: 72% EXTRACTED · 28% INFERRED · 0% AMBIGUOUS · INFERRED: 310 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `d26ecb1f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Backend
- backend.cpp
- safety.cpp
- health.cpp
- QVariantMap
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
- .savedRules
- appspace.cpp
- QVariantList
- Theme
- .write
- Backend::importAppSpace

## God Nodes (most connected - your core abstractions)
1. `Backend` - 154 edges
2. `WindowTunerTests` - 32 edges
3. `QVariantMap` - 31 edges
4. `QVariantList` - 20 edges
5. `Theme` - 13 edges
6. `WindowTuner one-stop implementation report` - 13 edges
7. `Backend::restoreSnapshot()` - 12 edges
8. `WindowTuner implementation checklist` - 11 edges
9. `Backend::Backend()` - 10 edges
10. `Backend::importAppSpace()` - 9 edges

## Surprising Connections (you probably didn't know these)
- `main()` --references--> `Backend`  [INFERRED]
  src/main.cpp → src/backend.h
- `Backend::validationDetails()` --references--> `QVariantMap`  [EXTRACTED]
  src/safety.cpp → src/backend.h
- `Backend::applicationCatalog()` --references--> `QVariantList`  [EXTRACTED]
  src/appspace.cpp → src/backend.h
- `Backend::applicationSettings()` --references--> `QVariantMap`  [EXTRACTED]
  src/appspace.cpp → src/backend.h
- `Backend::setApplicationPlacement()` --references--> `QVariantMap`  [EXTRACTED]
  src/appspace.cpp → src/backend.h

## Import Cycles
- None detected.

## Communities (30 total, 13 thin omitted)

### Community 0 - "Backend"
Cohesion: 0.05
Nodes (27): Backend, m_backups, m_configDirectory, m_diskFingerprint, m_future, m_hasRefreshed, m_hyprctl, m_hyprlandConfig (+19 more)

### Community 1 - "backend.cpp"
Cohesion: 0.11
Nodes (20): Backend::commitPlacementData(), Backend::setApplicationPlacement(), Backend::Backend(), Backend::loadRuleForEditing(), writeAtomically(), Backend::configureEntryPoint(), Backend::batchSetEnabled(), Backend::exportSelected() (+12 more)

### Community 2 - "safety.cpp"
Cohesion: 0.18
Nodes (11): atomic(), Backend::createBackup(), Backend::fingerprint(), Backend::redo(), Backend::restoreBackup(), Backend::saveJournal(), Backend::snapshot(), Backend::summarize() (+3 more)

### Community 3 - "health.cpp"
Cohesion: 0.13
Nodes (4): Theme::Theme(), LiveGeometryTests, DesktopFixture, dir

### Community 4 - "QVariantMap"
Cohesion: 0.12
Nodes (17): QStringList, QVariantMap, luaString(), luaValue(), number(), patternFor(), regexEscape(), Backend::diagnosticReport() (+9 more)

### Community 5 - "workflows.cpp"
Cohesion: 0.18
Nodes (9): Backend::loadLibrary(), Backend::preference(), Backend::presetSettings(), Backend::profilePreview(), Backend::removeLibraryItem(), Backend::savePreset(), Backend::setPreference(), Backend::writeLibrary() (+1 more)

### Community 6 - "WindowTuner one-stop implementation report"
Cohesion: 0.06
Nodes (30): 1. Safety and recovery, 2. Visual placement, 3. Inspector and match testing, 4. Presets and profiles, 5. Applications and rule management, 6. Compatibility and health, 7. Beginner experience, 8. Apps, workspaces and displays (AppSpace parity) (+22 more)

### Community 9 - "src/monitor_recovery.lua"
Cohesion: 0.23
Nodes (11): clamp(), debounce(), dispatch(), geometry(), number(), place(), reconcileNow(), schedule() (+3 more)

### Community 10 - "QString"
Cohesion: 0.15
Nodes (14): Backend::applicationCatalog(), Backend::applicationSettings(), Backend::appSpaceImportPath(), Backend::appSpacePath(), Backend::deriveApplicationClass(), Backend::desktopExecCommand(), Backend::desktopLaunchTarget(), browserPrefix() (+6 more)

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

### Community 23 - "appspace.cpp"
Cohesion: 0.14
Nodes (7): Backend::appSpaceModuleConfig(), Backend::keepMonitorLayout(), Backend::monitorData(), Backend::workspaceData(), isPlacementOnly(), quoted(), Backend::decodeRules()

### Community 24 - "QVariantList"
Cohesion: 0.11
Nodes (12): Backend::monitorPlan(), Backend::workspacePlan(), QVariantList, Backend::auditCapabilities(), Backend::externalRuleHints(), Backend::installedApplications(), Backend::placementGeometry(), Backend::resolvePlacement() (+4 more)

### Community 26 - "Theme"
Cohesion: 0.20
Nodes (5): Theme, m_accent, m_debounce, m_directory, m_watcher

### Community 28 - "Backend::importAppSpace"
Cohesion: 0.36
Nodes (6): Backend::applyMonitorLayout(), Backend::appSpaceImportPreview(), Backend::importAppSpace(), Backend::removeWorkspacePlan(), Backend::revertMonitorLayout(), Backend::setWorkspacePlan()

## Knowledge Gaps
- **31 isolated node(s):** `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl`, `m_rulePath`, `m_windows` (+26 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 138 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **13 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Backend` connect `Backend` to `backend.cpp`, `safety.cpp`, `QVariantMap`, `workflows.cpp`, `.savesAndLoadsPersistentRule`, `QString`, `WindowTunerTests`, `.importsAppSpacePlacementDisplaysAndAliases`, `.monitorLayoutKeepsOrReverts`, `.savedRules`, `QVariantList`, `backend.h`, `.write`, `Backend::importAppSpace`, `.healthAndDiagnosticsDoNotLeakPrivateData`?**
  _High betweenness centrality (0.324) - this node is a cross-community bridge._
- **Are the 11 inferred relationships involving `Backend` (e.g. with `main()` and `.auditsInstalledFeatureCapabilities()`) actually correct?**
  _`Backend` has 11 INFERRED edges - model-reasoned connections that need verification._
- **What connects `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl` to the rest of the system?**
  _31 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Backend` be split into smaller, more focused modules?**
  _Cohesion score 0.045068027210884355 - nodes in this community are weakly interconnected._
- **Why does `QVariantMap` connect `QVariantMap` to `Backend`, `backend.cpp`, `safety.cpp`, `workflows.cpp`, `QString`, `QVariantList`, `backend.h`, `Backend::importAppSpace`?**
  _High betweenness centrality (0.035) - this node is a cross-community bridge._
- **Should `backend.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.1125886524822695 - nodes in this community are weakly interconnected._
- **Why does `WindowTunerTests` connect `WindowTunerTests` to `Backend`, `health.cpp`, `QVariantMap`, `.savesAndLoadsPersistentRule`, `QString`, `.importsAppSpacePlacementDisplaysAndAliases`, `.monitorLayoutKeepsOrReverts`, `.savedRules`, `QVariantList`, `.write`, `.healthAndDiagnosticsDoNotLeakPrivateData`?**
  _High betweenness centrality (0.034) - this node is a cross-community bridge._