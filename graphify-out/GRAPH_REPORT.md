# Graph Report - windowtuner  (2026-11-07)

## Corpus Check
- 13 files · ~5,201 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 7 file(s) not represented in the graph (top: (none) 2, .pro 2, .desktop 1)

## Summary
- 144 nodes · 282 edges · 12 communities (6 shown, 6 thin omitted)
- Extraction: 80% EXTRACTED · 20% INFERRED · 0% AMBIGUOUS · INFERRED: 55 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- Backend
- backend.cpp
- Theme
- theme.cpp
- .luaRule
- .parseClients
- WindowTuner implementation checklist
- WindowTuner
- tui.tsx

## God Nodes (most connected - your core abstractions)
1. `Backend` - 52 edges
2. `Theme` - 13 edges
3. `QVariantMap` - 10 edges
4. `WindowTuner implementation checklist` - 8 edges
5. `writeAtomically()` - 7 edges
6. `WindowTunerTests` - 7 edges
7. `luaString()` - 5 edges
8. `luaValue()` - 5 edges
9. `number()` - 5 edges
10. `Backend::Backend()` - 5 edges

## Surprising Connections (you probably didn't know these)
- `number()` --references--> `QVariantMap`  [EXTRACTED]
  src/backend.cpp → src/backend.h
- `Backend::loadRuleForEditing()` --references--> `QVariantMap`  [EXTRACTED]
  src/backend.cpp → src/backend.h

## Import Cycles
- None detected.

## Communities (12 total, 6 thin omitted)

### Community 0 - "Backend"
Cohesion: 0.09
Nodes (11): Backend, m_configDirectory, m_hasRefreshed, m_hyprctl, m_hyprlandConfig, m_refreshTimer, m_rulePath, m_savedRules (+3 more)

### Community 2 - "Theme"
Cohesion: 0.20
Nodes (5): Theme, m_accent, m_debounce, m_directory, m_watcher

### Community 4 - ".luaRule"
Cohesion: 0.29
Nodes (7): Backend::loadRuleForEditing(), QVariantMap, luaString(), luaValue(), number(), patternFor(), regexEscape()

### Community 6 - "WindowTuner implementation checklist"
Cohesion: 0.22
Nodes (8): Appearance and behavior, Layout and placement, Matching and scope, OpenCode progress tracking, Safety, validation, and polish, Saved rules and data, Window picker and discovery, WindowTuner implementation checklist

### Community 7 - "WindowTuner"
Cohesion: 0.50
Nodes (3): Build and run, Feature categories, WindowTuner

### Community 11 - "tui.tsx"
Cohesion: 0.22
Nodes (4): readTodo(), setup(), TodoRow, TodoSidebar()

## Knowledge Gaps
- **14 isolated node(s):** `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl`, `m_rulePath`, `m_windows` (+9 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 68 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **6 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Backend` connect `Backend` to `backend.cpp`, `theme.cpp`, `.luaRule`, `.parseClients`?**
  _High betweenness centrality (0.303) - this node is a cross-community bridge._
- **What connects `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl` to the rest of the system?**
  _14 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Backend` be split into smaller, more focused modules?**
  _Cohesion score 0.09116809116809117 - nodes in this community are weakly interconnected._
- **Why does `Theme` connect `Theme` to `theme.cpp`?**
  _High betweenness centrality (0.108) - this node is a cross-community bridge._
- **Should `theme.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.1067193675889328 - nodes in this community are weakly interconnected._
- **Why does `WindowTunerTests` connect `.parseClients` to `backend.cpp`, `theme.cpp`?**
  _High betweenness centrality (0.019) - this node is a cross-community bridge._