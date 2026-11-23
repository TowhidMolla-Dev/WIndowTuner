# Graph Report - windowtuner  (2026-11-24)

## Corpus Check
- 39 files · ~37,528 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 18 file(s) not represented in the graph (top: .qml 11, .pro 3, (none) 2)

## Summary
- 609 nodes · 1304 edges · 32 communities (20 shown, 12 thin omitted)
- Extraction: 74% EXTRACTED · 26% INFERRED · 0% AMBIGUOUS · INFERRED: 345 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `16636f6f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- WindowTuner
- QVariantMap
- safety.cpp
- windowtuner_tests.cpp
- health.cpp
- workflows.cpp
- WindowTuner one-stop implementation report
- workflows.cpp
- src/monitor_recovery.lua
- tui.tsx
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
- Dr. Unslopovich's diagnosis: whole app
- Treatment: dialogs and workflows
- Treatment: main UI redesign

## God Nodes (most connected - your core abstractions)
1. `Backend` - 175 edges
2. `QVariantMap` - 36 edges
3. `WindowTunerTests` - 34 edges
4. `QVariantList` - 22 edges
5. `Theme` - 13 edges
6. `WindowTuner one-stop implementation report` - 13 edges
7. `Backend::restoreSnapshot()` - 12 edges
8. `Backend::Backend()` - 11 edges
9. `WindowTuner` - 11 edges
10. `WindowTuner implementation checklist` - 11 edges

## Surprising Connections (you probably didn't know these)
- `Omaplace comparison: UX and feature fit for WindowTuner` --references--> `main()`  [INFERRED]
  docs/omaplace-ux-feature-assessment.md → src/main.cpp
- `main()` --references--> `Backend`  [INFERRED]
  src/main.cpp → src/backend.h
- `Backend::validationDetails()` --references--> `QVariantMap`  [EXTRACTED]
  src/safety.cpp → src/backend.h
- `Backend::auditCapabilities()` --references--> `QVariantList`  [EXTRACTED]
  src/health.cpp → src/backend.h
- `Backend::externalRuleHints()` --references--> `QVariantList`  [EXTRACTED]
  src/inspector.cpp → src/backend.h

## Import Cycles
- None detected.

## Communities (32 total, 12 thin omitted)

### Community 0 - "WindowTuner"
Cohesion: 0.12
Nodes (16): Apps, workspaces, and displays, Build and install from source, Build and test from source, Built to be reversible, Configuration and privacy, Get started, Keyboard shortcuts, Known boundaries (+8 more)

### Community 1 - "QVariantMap"
Cohesion: 0.06
Nodes (17): QVariantMap, Backend::auditCapabilities(), Backend::diagnosticReport(), Backend::healthReport(), Backend::verifyDraft(), query(), Backend::externalRuleHints(), Backend::inspectWindow() (+9 more)

### Community 2 - "safety.cpp"
Cohesion: 0.04
Nodes (30): Backend, m_backups, m_configDirectory, m_diskFingerprint, m_eventBuffer, m_eventSocket, m_future, m_hasRefreshed (+22 more)

### Community 3 - "windowtuner_tests.cpp"
Cohesion: 0.07
Nodes (36): Backend::applicationCatalog(), Backend::applicationSettings(), Backend::applyMonitorLayout(), Backend::appSpaceImportPath(), Backend::appSpaceImportPreview(), Backend::appSpaceModuleConfig(), Backend::appSpacePath(), Backend::clearApplicationPlacement() (+28 more)

### Community 4 - "health.cpp"
Cohesion: 0.05
Nodes (38): name, runs-on, steps, APPIMAGE_EXTRACT_AND_RUN, EXTRA_PLATFORM_PLUGINS, LINUXDEPLOY, QMAKE, RELEASE_TAG (+30 more)

### Community 5 - "workflows.cpp"
Cohesion: 0.12
Nodes (19): QStringList, Backend::configureEntryPoint(), atomic(), Backend::~Backend(), Backend::createBackup(), Backend::fingerprint(), Backend::redo(), Backend::restoreBackup() (+11 more)

### Community 6 - "WindowTuner one-stop implementation report"
Cohesion: 0.05
Nodes (39): Architecture and integration risks, Executive summary, Feature comparison, Implementation status, Omaplace comparison: UX and feature fit for WindowTuner, Phase 1 — Desktop overview (small/medium), Phase 2 — Named scene capture/open (large), Phase 3 — Session restore and optional automation (large/high risk) (+31 more)

### Community 7 - "workflows.cpp"
Cohesion: 0.08
Nodes (29): QVariantList, Backend::placementGeometry(), Backend::backups(), Backend::history(), Backend::activateProfile(), Backend::captureScene(), Backend::loadLibrary(), Backend::openScene() (+21 more)

### Community 11 - "tui.tsx"
Cohesion: 0.23
Nodes (11): clamp(), debounce(), dispatch(), geometry(), number(), place(), reconcileNow(), schedule() (+3 more)

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
Cohesion: 0.09
Nodes (13): Backend::Backend(), Backend::loadRuleForEditing(), luaString(), luaValue(), number(), patternFor(), regexEscape(), writeAtomically() (+5 more)

### Community 30 - "Dr. Unslopovich's diagnosis: whole app"
Cohesion: 0.22
Nodes (8): Ask first, Dr. Unslopovich's diagnosis: whole app, Follow-up implementation and verification, Minor, Not checked, Noticeable, Recommended whole-app direction, Worth keeping

### Community 31 - "Treatment: dialogs and workflows"
Cohesion: 0.33
Nodes (5): Changes, Not checked, Self-check, Treatment: dialogs and workflows, Verification

### Community 33 - "Treatment: main UI redesign"
Cohesion: 0.33
Nodes (5): Changes, Direction, Not checked, Treatment: main UI redesign, Verification

## Knowledge Gaps
- **34 isolated node(s):** `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl`, `m_rulePath`, `m_windows` (+29 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 222 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **12 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Backend` connect `safety.cpp` to `QVariantMap`, `windowtuner_tests.cpp`, `workflows.cpp`, `WindowTuner one-stop implementation report`, `workflows.cpp`, `.savesAndLoadsPersistentRule`, `src/monitor_recovery.lua`, `.monitorLayoutKeepsOrReverts`, `WindowTunerTests`, `one-stop-roadmap.md`, `QVariantMap`, `.healthAndDiagnosticsDoNotLeakPrivateData`?**
  _High betweenness centrality (0.351) - this node is a cross-community bridge._
- **Are the 14 inferred relationships involving `Backend` (e.g. with `main()` and `.auditsInstalledFeatureCapabilities()`) actually correct?**
  _`Backend` has 14 INFERRED edges - model-reasoned connections that need verification._
- **What connects `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl` to the rest of the system?**
  _34 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `WindowTuner` be split into smaller, more focused modules?**
  _Cohesion score 0.125 - nodes in this community are weakly interconnected._
- **Why does `main()` connect `WindowTuner one-stop implementation report` to `QVariantMap`, `safety.cpp`, `QVariantMap`?**
  _High betweenness centrality (0.131) - this node is a cross-community bridge._
- **Should `QVariantMap` be split into smaller, more focused modules?**
  _Cohesion score 0.05734767025089606 - nodes in this community are weakly interconnected._
- **Should `safety.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.03773584905660377 - nodes in this community are weakly interconnected._