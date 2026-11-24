# Graph Report - windowtuner  (2026-11-24)

## Corpus Check
- 41 files · ~44,488 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 25 file(s) not represented in the graph (top: .qml 18, .pro 3, (none) 2)

## Summary
- 690 nodes · 1553 edges · 41 communities (28 shown, 13 thin omitted)
- Extraction: 72% EXTRACTED · 28% INFERRED · 0% AMBIGUOUS · INFERRED: 442 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `772163b0`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- .healthAndDiagnosticsDoNotLeakPrivateData
- backend.h
- safety.cpp
- health.cpp
- .setStatus
- QVariantList
- backend.cpp
- WindowTuner
- QVariantMap
- tui.tsx
- WindowTuner implementation checklist
- appspace_runtime.lua
- cli_smoke.py
- appspace.lua
- install
- Theme
- Feature tour
- QVariantList
- Dr. Unslopovich's diagnosis
- Follow-up: main UI
- Follow-up: live Windows screen
- migration.cpp
- .private
- Dr. Unslopovich's diagnosis: whole app
- Treatment: dialogs and workflows
- appspace.cpp
- Treatment: main UI redesign
- Treatment: window preview
- Omaplace comparison: UX and feature fit for WindowTuner
- WindowTuner implementation checklist
- WindowTunerTests
- WindowTuner one-stop implementation report
- Get started
- Suggested implementation sequence

## God Nodes (most connected - your core abstractions)
1. `Backend` - 196 edges
2. `QVariantMap` - 47 edges
3. `WindowTunerTests` - 40 edges
4. `QVariantList` - 23 edges
5. `Backend::restoreSnapshot()` - 14 edges
6. `Theme` - 13 edges
7. `WindowTuner one-stop implementation report` - 13 edges
8. `Backend::prepareMigration()` - 12 edges
9. `WindowTuner` - 12 edges
10. `Backend::Backend()` - 11 edges

## Surprising Connections (you probably didn't know these)
- `Omaplace comparison: UX and feature fit for WindowTuner` --references--> `main()`  [INFERRED]
  docs/omaplace-ux-feature-assessment.md → src/main.cpp
- `main()` --references--> `Backend`  [INFERRED]
  src/main.cpp → src/backend.h
- `Backend::validationDetails()` --references--> `QVariantMap`  [EXTRACTED]
  src/safety.cpp → src/backend.h
- `Backend::auditCapabilities()` --references--> `QVariantList`  [EXTRACTED]
  src/health.cpp → src/backend.h
- `Backend::applicationCatalog()` --references--> `QVariantList`  [EXTRACTED]
  src/appspace.cpp → src/backend.h

## Import Cycles
- None detected.

## Communities (41 total, 13 thin omitted)

### Community 1 - "backend.h"
Cohesion: 0.07
Nodes (8): Backend::auditCapabilities(), Backend::diagnosticReport(), Backend::healthReport(), query(), Theme::Theme(), LiveGeometryTests, DesktopFixture, dir

### Community 2 - "safety.cpp"
Cohesion: 0.03
Nodes (33): Backend, m_backups, m_configDirectory, m_diskFingerprint, m_dismissedGeometryRules, m_eventBuffer, m_eventSocket, m_future (+25 more)

### Community 4 - "health.cpp"
Cohesion: 0.05
Nodes (38): name, runs-on, steps, APPIMAGE_EXTRACT_AND_RUN, EXTRA_PLATFORM_PLUGINS, LINUXDEPLOY, QMAKE, RELEASE_TAG (+30 more)

### Community 5 - ".setStatus"
Cohesion: 0.06
Nodes (50): QStringList, Backend::configureEntryPoint(), Backend::setGeometrySuggestionsEnabled(), Backend::applyMigration(), atomic(), Backend::~Backend(), Backend::createBackup(), Backend::fingerprint() (+42 more)

### Community 6 - "QVariantList"
Cohesion: 0.11
Nodes (17): Backend::monitorPlan(), Backend::workspacePlan(), QVariantList, Backend::externalRuleHints(), Backend::decodeRules(), Backend::exportSelected(), Backend::importSelection(), Backend::installedApplications() (+9 more)

### Community 7 - "backend.cpp"
Cohesion: 0.10
Nodes (9): Backend::Backend(), luaString(), luaValue(), number(), patternFor(), regexEscape(), writeAtomically(), Backend::batchSetEnabled() (+1 more)

### Community 9 - "WindowTuner"
Cohesion: 0.17
Nodes (10): Build and test from source, Built to be reversible, Configuration and privacy, Keyboard shortcuts, Known boundaries, License, Make every window behave the way you want., Read-only command line (+2 more)

### Community 10 - "QVariantMap"
Cohesion: 0.13
Nodes (15): Backend::editorStateForWindow(), Backend::loadRuleForEditing(), QVariantMap, Backend::verifyDraft(), Backend::inspectWindow(), Backend::interpretRuleRequest(), Backend::matchWindow(), Backend::observeGeometryChanges() (+7 more)

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

### Community 20 - "Feature tour"
Cohesion: 0.18
Nodes (11): Apps, workspaces, and displays, Describe a rule, Discover an app’s class, Feature tour, Inspect and organize rules, Map desktops and restore scenes, Remember geometry changes, Safety, compatibility, and reusable workflows (+3 more)

### Community 25 - "Dr. Unslopovich's diagnosis"
Cohesion: 0.29
Nodes (6): Ask first, Dr. Unslopovich's diagnosis, Minor, Not checked, Noticeable, Worth keeping

### Community 26 - "Follow-up: main UI"
Cohesion: 0.50
Nodes (3): Comparison with 2026-11-18-1624, Follow-up: main UI, Not checked

### Community 27 - "Follow-up: live Windows screen"
Cohesion: 0.50
Nodes (3): Comparison with 2026-11-18-1624, Follow-up: live Windows screen, Not checked

### Community 28 - "migration.cpp"
Cohesion: 0.16
Nodes (12): Backend::exportMigration(), Backend::prepareMigration(), Backend::previewMigration(), configuration(), derive(), Key, bytes, portableRules() (+4 more)

### Community 30 - "Dr. Unslopovich's diagnosis: whole app"
Cohesion: 0.22
Nodes (8): Ask first, Dr. Unslopovich's diagnosis: whole app, Follow-up implementation and verification, Minor, Not checked, Noticeable, Recommended whole-app direction, Worth keeping

### Community 31 - "Treatment: dialogs and workflows"
Cohesion: 0.33
Nodes (5): Changes, Not checked, Self-check, Treatment: dialogs and workflows, Verification

### Community 32 - "appspace.cpp"
Cohesion: 0.10
Nodes (29): Backend::applicationCatalog(), Backend::applicationSettings(), Backend::applyMonitorLayout(), Backend::appSpaceImportPath(), Backend::appSpaceImportPreview(), Backend::appSpaceModuleConfig(), Backend::appSpacePath(), Backend::clearApplicationPlacement() (+21 more)

### Community 33 - "Treatment: main UI redesign"
Cohesion: 0.33
Nodes (5): Changes, Direction, Not checked, Treatment: main UI redesign, Verification

### Community 34 - "Treatment: window preview"
Cohesion: 0.25
Nodes (7): Changes, Direction, Not checked, Score, Treatment self-check, Treatment: window preview, Verification

### Community 35 - "Omaplace comparison: UX and feature fit for WindowTuner"
Cohesion: 0.18
Nodes (9): Architecture and integration risks, Executive summary, Feature comparison, Implementation status, Omaplace comparison: UX and feature fit for WindowTuner, Sources, Verdict, What Omaplace is (+1 more)

### Community 36 - "WindowTuner implementation checklist"
Cohesion: 0.18
Nodes (11): Appearance and behavior, Apps, workspaces and displays (AppSpace parity), Interface polish, Layout and placement, Matching and scope, OpenCode progress tracking, Remaining boundaries, Safety, validation, and polish (+3 more)

### Community 38 - "WindowTuner one-stop implementation report"
Cohesion: 0.14
Nodes (14): 1. Safety and recovery, 2. Visual placement, 3. Inspector and match testing, 4. Presets and profiles, 5. Applications and rule management, 6. Compatibility and health, 7. Beginner experience, 8. Apps, workspaces and displays (AppSpace parity) (+6 more)

### Community 39 - "Get started"
Cohesion: 0.40
Nodes (5): Build and install from source, Get started, Run the AppImage, What you need, Your first rule

### Community 40 - "Suggested implementation sequence"
Cohesion: 0.50
Nodes (4): Phase 1 — Desktop overview (small/medium), Phase 2 — Named scene capture/open (large), Phase 3 — Session restore and optional automation (large/high risk), Suggested implementation sequence

## Knowledge Gaps
- **38 isolated node(s):** `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl`, `m_rulePath`, `m_windows` (+33 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 249 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **13 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Backend` connect `safety.cpp` to `appspace.cpp`, `backend.h`, `.healthAndDiagnosticsDoNotLeakPrivateData`, `.controlStyleNavigationAndResponsiveLayout`, `WindowTunerTests`, `.setStatus`, `backend.cpp`, `.savesAndLoadsPersistentRule`, `QVariantList`, `QVariantMap`, `.importsAppSpacePlacementDisplaysAndAliases`, `.write`, `migration.cpp`, `.private`?**
  _High betweenness centrality (0.361) - this node is a cross-community bridge._
- **Are the 16 inferred relationships involving `Backend` (e.g. with `main()` and `.auditsInstalledFeatureCapabilities()`) actually correct?**
  _`Backend` has 16 INFERRED edges - model-reasoned connections that need verification._
- **What connects `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl` to the rest of the system?**
  _38 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `backend.h` be split into smaller, more focused modules?**
  _Cohesion score 0.0730804810360777 - nodes in this community are weakly interconnected._
- **Why does `main()` connect `.healthAndDiagnosticsDoNotLeakPrivateData` to `backend.h`, `safety.cpp`, `Omaplace comparison: UX and feature fit for WindowTuner`, `backend.cpp`?**
  _High betweenness centrality (0.137) - this node is a cross-community bridge._
- **Should `safety.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.03373015873015873 - nodes in this community are weakly interconnected._
- **Why does `Omaplace comparison: UX and feature fit for WindowTuner` connect `Omaplace comparison: UX and feature fit for WindowTuner` to `Suggested implementation sequence`, `.healthAndDiagnosticsDoNotLeakPrivateData`?**
  _High betweenness centrality (0.137) - this node is a cross-community bridge._