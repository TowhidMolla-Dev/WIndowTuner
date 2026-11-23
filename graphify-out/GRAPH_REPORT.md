# Graph Report - windowtuner  (2026-11-24)

## Corpus Check
- 39 files · ~38,320 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 18 file(s) not represented in the graph (top: .qml 11, .pro 3, (none) 2)

## Summary
- 616 nodes · 1315 edges · 39 communities (26 shown, 13 thin omitted)
- Extraction: 74% EXTRACTED · 26% INFERRED · 0% AMBIGUOUS · INFERRED: 347 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `f8fa2aa6`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- WindowTuner one-stop implementation report
- QVariantMap
- safety.cpp
- windowtuner_tests.cpp
- health.cpp
- workflows.cpp
- Omaplace comparison: UX and feature fit for WindowTuner
- .setStatus
- .savesAndLoadsPersistentRule
- WindowTunerTests
- .readRuleData
- tui.tsx
- WindowTunerTests
- WindowTuner
- WindowTuner implementation checklist
- appspace_runtime.lua
- cli_smoke.py
- appspace.lua
- install
- Theme
- QVariantList
- Dr. Unslopovich's diagnosis
- Follow-up: main UI
- Follow-up: live Windows screen
- QVariantMap
- .healthAndDiagnosticsDoNotLeakPrivateData
- Dr. Unslopovich's diagnosis: whole app
- Treatment: dialogs and workflows
- WindowTuner implementation checklist
- Treatment: main UI redesign
- Feature tour
- Get started
- Suggested implementation sequence

## God Nodes (most connected - your core abstractions)
1. `Backend` - 175 edges
2. `QVariantMap` - 38 edges
3. `WindowTunerTests` - 34 edges
4. `QVariantList` - 22 edges
5. `Theme` - 13 edges
6. `WindowTuner one-stop implementation report` - 13 edges
7. `Backend::restoreSnapshot()` - 12 edges
8. `WindowTuner` - 12 edges
9. `Backend::Backend()` - 11 edges
10. `WindowTuner implementation checklist` - 11 edges

## Surprising Connections (you probably didn't know these)
- `Omaplace comparison: UX and feature fit for WindowTuner` --references--> `main()`  [INFERRED]
  docs/omaplace-ux-feature-assessment.md → src/main.cpp
- `main()` --references--> `Backend`  [INFERRED]
  src/main.cpp → src/backend.h
- `Backend::externalRuleHints()` --references--> `QVariantList`  [EXTRACTED]
  src/inspector.cpp → src/backend.h
- `Backend::applicationCatalog()` --references--> `QVariantList`  [EXTRACTED]
  src/appspace.cpp → src/backend.h
- `Backend::setApplicationPlacement()` --references--> `QVariantMap`  [EXTRACTED]
  src/appspace.cpp → src/backend.h

## Import Cycles
- None detected.

## Communities (39 total, 13 thin omitted)

### Community 0 - "WindowTuner one-stop implementation report"
Cohesion: 0.14
Nodes (14): 1. Safety and recovery, 2. Visual placement, 3. Inspector and match testing, 4. Presets and profiles, 5. Applications and rule management, 6. Compatibility and health, 7. Beginner experience, 8. Apps, workspaces and displays (AppSpace parity) (+6 more)

### Community 1 - "QVariantMap"
Cohesion: 0.08
Nodes (3): Theme::Theme(), LiveGeometryTests, visualItem()

### Community 2 - "safety.cpp"
Cohesion: 0.04
Nodes (30): Backend, m_backups, m_configDirectory, m_diskFingerprint, m_eventBuffer, m_eventSocket, m_future, m_hasRefreshed (+22 more)

### Community 3 - "windowtuner_tests.cpp"
Cohesion: 0.09
Nodes (25): Backend::applicationCatalog(), Backend::applyMonitorLayout(), Backend::appSpaceImportPath(), Backend::appSpaceImportPreview(), Backend::appSpaceModuleConfig(), Backend::appSpacePath(), Backend::deriveApplicationClass(), Backend::desktopExecCommand() (+17 more)

### Community 4 - "health.cpp"
Cohesion: 0.05
Nodes (38): name, runs-on, steps, APPIMAGE_EXTRACT_AND_RUN, EXTRA_PLATFORM_PLUGINS, LINUXDEPLOY, QMAKE, RELEASE_TAG (+30 more)

### Community 5 - "workflows.cpp"
Cohesion: 0.09
Nodes (22): Backend::Backend(), QStringList, writeAtomically(), Backend::configureEntryPoint(), Backend::diagnosticReport(), atomic(), Backend::~Backend(), Backend::createBackup() (+14 more)

### Community 6 - "Omaplace comparison: UX and feature fit for WindowTuner"
Cohesion: 0.18
Nodes (9): Architecture and integration risks, Executive summary, Feature comparison, Implementation status, Omaplace comparison: UX and feature fit for WindowTuner, Sources, Verdict, What Omaplace is (+1 more)

### Community 7 - ".setStatus"
Cohesion: 0.11
Nodes (24): Backend::commitPlacementData(), Backend::keepMonitorLayout(), Backend::activateProfile(), Backend::applyResolvedScene(), Backend::captureScene(), Backend::loadLibrary(), Backend::openScene(), Backend::preference() (+16 more)

### Community 10 - ".readRuleData"
Cohesion: 0.10
Nodes (18): Backend::monitorPlan(), Backend::workspacePlan(), QVariantList, Backend::auditCapabilities(), Backend::batchSetEnabled(), Backend::decodeRules(), Backend::exportSelected(), Backend::importSelection() (+10 more)

### Community 11 - "tui.tsx"
Cohesion: 0.23
Nodes (11): clamp(), debounce(), dispatch(), geometry(), number(), place(), reconcileNow(), schedule() (+3 more)

### Community 13 - "WindowTuner"
Cohesion: 0.17
Nodes (10): Build and test from source, Built to be reversible, Configuration and privacy, Keyboard shortcuts, Known boundaries, License, Make every window behave the way you want., Read-only command line (+2 more)

### Community 14 - "WindowTuner implementation checklist"
Cohesion: 0.22
Nodes (4): readTodo(), setup(), TodoRow, TodoSidebar()

### Community 15 - "appspace_runtime.lua"
Cohesion: 0.38
Nodes (5): block_of(), move_to(), owner(), plain(), slot_id()

### Community 17 - "appspace.lua"
Cohesion: 0.36
Nodes (5): environment(), monitor(), roots(), tempdir(), writefile()

### Community 19 - "Theme"
Cohesion: 0.11
Nodes (12): Theme, m_accent, m_debounce, m_directory, m_watcher, Corrected from earlier visits, Dr. Unslopovich's diagnosis: dialogs and workflows, Minor (+4 more)

### Community 25 - "Dr. Unslopovich's diagnosis"
Cohesion: 0.29
Nodes (6): Ask first, Dr. Unslopovich's diagnosis, Minor, Not checked, Noticeable, Worth keeping

### Community 26 - "Follow-up: main UI"
Cohesion: 0.50
Nodes (3): Comparison with 2026-11-18-1624, Follow-up: main UI, Not checked

### Community 27 - "Follow-up: live Windows screen"
Cohesion: 0.50
Nodes (3): Comparison with 2026-11-18-1624, Follow-up: live Windows screen, Not checked

### Community 28 - "QVariantMap"
Cohesion: 0.08
Nodes (22): Backend::applicationSettings(), Backend::editorStateForWindow(), Backend::loadRuleForEditing(), QVariantMap, luaString(), luaValue(), number(), patternFor() (+14 more)

### Community 30 - "Dr. Unslopovich's diagnosis: whole app"
Cohesion: 0.22
Nodes (8): Ask first, Dr. Unslopovich's diagnosis: whole app, Follow-up implementation and verification, Minor, Not checked, Noticeable, Recommended whole-app direction, Worth keeping

### Community 31 - "Treatment: dialogs and workflows"
Cohesion: 0.33
Nodes (5): Changes, Not checked, Self-check, Treatment: dialogs and workflows, Verification

### Community 32 - "WindowTuner implementation checklist"
Cohesion: 0.18
Nodes (11): Appearance and behavior, Apps, workspaces and displays (AppSpace parity), Interface polish, Layout and placement, Matching and scope, OpenCode progress tracking, Remaining boundaries, Safety, validation, and polish (+3 more)

### Community 33 - "Treatment: main UI redesign"
Cohesion: 0.33
Nodes (5): Changes, Direction, Not checked, Treatment: main UI redesign, Verification

### Community 36 - "Feature tour"
Cohesion: 0.33
Nodes (6): Apps, workspaces, and displays, Feature tour, Inspect and organize rules, Map desktops and restore scenes, Safety, compatibility, and reusable workflows, Shape and position windows

### Community 37 - "Get started"
Cohesion: 0.40
Nodes (5): Build and install from source, Get started, Run the AppImage, What you need, Your first rule

### Community 38 - "Suggested implementation sequence"
Cohesion: 0.50
Nodes (4): Phase 1 — Desktop overview (small/medium), Phase 2 — Named scene capture/open (large), Phase 3 — Session restore and optional automation (large/high risk), Suggested implementation sequence

## Knowledge Gaps
- **34 isolated node(s):** `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl`, `m_rulePath`, `m_windows` (+29 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 227 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **13 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Backend` connect `safety.cpp` to `QVariantMap`, `.selectedWindow`, `.monitorLayoutKeepsOrReverts`, `windowtuner_tests.cpp`, `workflows.cpp`, `.setStatus`, `.savesAndLoadsPersistentRule`, `WindowTunerTests`, `.readRuleData`, `WindowTunerTests`, `.saveRule`, `QVariantMap`, `.healthAndDiagnosticsDoNotLeakPrivateData`?**
  _High betweenness centrality (0.353) - this node is a cross-community bridge._
- **Are the 14 inferred relationships involving `Backend` (e.g. with `main()` and `.auditsInstalledFeatureCapabilities()`) actually correct?**
  _`Backend` has 14 INFERRED edges - model-reasoned connections that need verification._
- **What connects `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl` to the rest of the system?**
  _34 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `WindowTuner one-stop implementation report` be split into smaller, more focused modules?**
  _Cohesion score 0.14285714285714285 - nodes in this community are weakly interconnected._
- **Why does `main()` connect `.healthAndDiagnosticsDoNotLeakPrivateData` to `QVariantMap`, `safety.cpp`, `Omaplace comparison: UX and feature fit for WindowTuner`?**
  _High betweenness centrality (0.139) - this node is a cross-community bridge._
- **Should `QVariantMap` be split into smaller, more focused modules?**
  _Cohesion score 0.07656341320864991 - nodes in this community are weakly interconnected._
- **Why does `Omaplace comparison: UX and feature fit for WindowTuner` connect `Omaplace comparison: UX and feature fit for WindowTuner` to `.healthAndDiagnosticsDoNotLeakPrivateData`, `Suggested implementation sequence`?**
  _High betweenness centrality (0.139) - this node is a cross-community bridge._