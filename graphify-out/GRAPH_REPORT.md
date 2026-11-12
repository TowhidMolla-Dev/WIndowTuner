# Graph Report - windowtuner  (2026-11-07)

## Corpus Check
- 13 files · ~5,811 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 7 file(s) not represented in the graph (top: (none) 2, .pro 2, .desktop 1)

## Summary
- 156 nodes · 295 edges · 10 communities (5 shown, 5 thin omitted)
- Extraction: 81% EXTRACTED · 19% INFERRED · 0% AMBIGUOUS · INFERRED: 55 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `3ddf88ce`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Backend
- backend.cpp
- theme.cpp
- .luaRule
- .parseClients
- WindowTuner
- tui.tsx

## God Nodes (most connected - your core abstractions)
1. `Backend` - 52 edges
2. `Theme` - 13 edges
3. `QVariantMap` - 10 edges
4. `WindowTuner` - 9 edges
5. `WindowTuner implementation checklist` - 9 edges
6. `writeAtomically()` - 7 edges
7. `WindowTunerTests` - 7 edges
8. `luaString()` - 5 edges
9. `luaValue()` - 5 edges
10. `number()` - 5 edges

## Surprising Connections (you probably didn't know these)
- `Backend::loadRuleForEditing()` --references--> `QVariantMap`  [EXTRACTED]
  src/backend.cpp → src/backend.h
- `number()` --references--> `QVariantMap`  [EXTRACTED]
  src/backend.cpp → src/backend.h

## Import Cycles
- None detected.

## Communities (10 total, 5 thin omitted)

### Community 0 - "Backend"
Cohesion: 0.09
Nodes (11): Backend, m_configDirectory, m_hasRefreshed, m_hyprctl, m_hyprlandConfig, m_refreshTimer, m_rulePath, m_savedRules (+3 more)

### Community 3 - "theme.cpp"
Cohesion: 0.07
Nodes (6): Theme, m_accent, m_debounce, m_directory, m_watcher, Theme::Theme()

### Community 4 - ".luaRule"
Cohesion: 0.33
Nodes (6): QVariantMap, luaString(), luaValue(), number(), patternFor(), regexEscape()

### Community 6 - "WindowTuner"
Cohesion: 0.08
Nodes (23): Build, test, and run, Build window rules, Configuration and data, Discover and select windows, Install, Interface, Keyboard shortcuts, Known limitations (+15 more)

### Community 11 - "tui.tsx"
Cohesion: 0.22
Nodes (4): readTodo(), setup(), TodoRow, TodoSidebar()

## Knowledge Gaps
- **14 isolated node(s):** `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl`, `m_rulePath`, `m_windows` (+9 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 76 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **5 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Backend` connect `Backend` to `backend.cpp`, `theme.cpp`, `.luaRule`, `.parseClients`?**
  _High betweenness centrality (0.258) - this node is a cross-community bridge._
- **What connects `m_configDirectory`, `m_hyprlandConfig`, `m_hyprctl` to the rest of the system?**
  _14 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Backend` be split into smaller, more focused modules?**
  _Cohesion score 0.09116809116809117 - nodes in this community are weakly interconnected._
- **Should `theme.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.07308377896613191 - nodes in this community are weakly interconnected._
- **Should `WindowTuner` be split into smaller, more focused modules?**
  _Cohesion score 0.08 - nodes in this community are weakly interconnected._