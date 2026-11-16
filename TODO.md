# WindowTuner implementation checklist

This checklist tracks the requested feature roadmap. Checkboxes are updated as
features land and are verified. A project-local OpenCode TUI plugin at
`.opencode/plugins/windowtuner-tasks/tui.tsx` mirrors this checklist in the
native `sidebar.content` slot and watches the file for updates. See `README.md`
for the user-facing feature set.

## OpenCode progress tracking

- [x] Show the checked/unchecked WindowTuner checklist in OpenCode's native TUI sidebar slot.
- [x] Update the sidebar automatically when `TODO.md` changes.

## Interface polish

- [x] Apply a cohesive dark, theme-accented studio visual language with compact section navigation and clear preview/status hierarchy.
- [x] Adopt Omarchy Control-style sidebar navigation, header search, selected-window summary, and fixed bottom actions.
- [x] Capture the interface using isolated test data and verify layouts at normal display sizes.

## Window picker and discovery

- [x] Search current windows by title, class, and workspace.
- [x] Show class, title, workspace, monitor, and floating state.
- [x] Pick a window by clicking it on screen.
- [x] Keep window list current as windows open, close, and rename.
- [x] Show process ID, stable ID, app ID, geometry, fullscreen/maximized state.
- [ ] Configure native Hyprland window groups (distinct from discovery-list grouping).
- [x] Group open windows by application in the discovery list.
- [x] Configure offline application targets with reviewed desktop-entry class hints.

## Matching and scope

- [x] Match selected initial class and title safely by default.
- [ ] Configure match fields: class, title, app ID/xdg tag, PID/stable ID, XWayland, role/content/state.
- [x] Choose exact, contains, or regular-expression matching for class/title.
- [x] Add positive and negative match clauses (title exclusion).
- [x] Match by workspace; monitor exclusion still pending.
- [ ] Detect all overlapping rules and show which saved rule matches each window. (Exact duplicates and broad same-class overlaps are flagged conservatively.)
- [x] Test draft matches against live client data and inspect matching saved rules with requested-value conflict explanations.

## Layout and placement

- [x] Float/tile/current mode, center, size, position, min/max size.
- [x] Compact, picture-in-picture, and large presets.
- [x] Save and load user-defined settings presets without changing matching scope.
- [x] Edge/corner anchors and margins in the placement designer.
- [ ] Monitor selection, per-monitor geometry, and follow-monitor/workspace.
- [x] Monitor selection, scaled/rotated usable-area geometry and save-time fallback selection.
- [x] Percentage dimensions and aspect-ratio locking.
- [x] Opt-in runtime recovery that re-places opted-in windows on monitor disconnect/reconnect while the app is closed.
- [ ] Arbitrary dimension expressions and automatic follow-monitor/workspace policies.
- [ ] Fullscreen, maximize, pseudotile, and group effects.
- [ ] Remember last size and position per matching window.

## Appearance and behavior

- [x] Opacity, border width, rounding, blur, shadow, focus, animations, dimming, VRR.
- [ ] Border color/gradient and opacity override modes.
- [ ] Rounding power, animation style, blur x-ray, glow, and remaining visual effects.
- [ ] Follow-mouse, focus-on-activate, stay-focused, and focus policies.
- [ ] Idle/shortcut inhibition, screen-share privacy, and other supported window effects.

## Saved rules and data

- [x] Save, preview, list, and confirmed-delete rules.
- [x] Load a saved rule into the editor and update its existing record in place.
- [x] Duplicate, enable/disable, reorder, search, and filter rules.
- [x] Import/export rules as JSON; preset import/export still pending.
- [x] Per-rule apply to matching open windows.
- [ ] Per-rule reset and change history with undo.
- [x] Persistent configuration history with undo/redo and named snapshots.
- [x] Manual collection profiles with preview, confirmation and undo.
- [x] Names/folders/tags, selected-rule batch actions and selective merge/replace imports.

## Apps, workspaces and displays (AppSpace parity)

Scope: the model behind the AppSpace plugin (apps, workspaces, monitors), not its
Omarchy bar widget.

- [x] Searchable app list merging running windows, saved rules and desktop entries.
- [x] App classes derived from desktop entries (StartupWMClass, Steam ids, Omarchy
      TUI/web-app launchers, entry name), with derived classes marked inferred.
- [x] Per-app placement: workspace, silent switch, launch at login, login-only
      placement window; written into the existing validated rule model.
- [x] Workspace pins to a display by make/model/serial, always-present flag and
      per-workspace app listing.
- [x] Omarchy 4.5 slot resolution (block file, slot API fallback, hyprmoncfg
      stand-down) plus Super+N / Super+Shift+N / Super+Shift+Alt+N rebinds.
- [x] Display editor for mode, position, scale, rotation and enabled state, saved
      by description, with a 15-second keep/revert and baseline restore.
- [x] AppSpace schema-6 import with preview, alias resolution and idempotent
      re-import of rules, pins, aliases and display layouts.
- [x] Config-error baseline: only errors introduced by a change roll back.
- [x] Runtime (`src/appspace_runtime.lua`) embedded in the generated module so
      pins, layout, login placement and autostart work while the app is closed.
- [x] Regression coverage: 18 Lua runtime scenarios, import/pin/display tests,
      dialog tabs rendered at three sizes, full module accepted by the installed
      Hyprland parser.
- [ ] Bar widget, workspace strip, drag-and-drop display canvas, app-class
      verification by launching apps, and mutating CLI commands (not ported).

## Safety, validation, and polish

- [x] Atomic rule writes, first-edit backup, and class/title preview.
- [x] Validate config reload and report Hyprland errors.
- [x] Roll back generated config on failed validation; manual backup/restore controls pending.
- [x] Read-only change review with module/main-config before/after views.
- [x] Installed-parser draft verification and ten-group capability audit.
- [x] Read-only CLI health, diagnostics, inspection, export and draft verification.
- [x] Dirty-draft guards, field validation and external-edit protection.
- [x] Beginner walkthrough, Basic/Advanced mode, settings search and interface scaling.
- [x] Add regression tests for matching, persistence, import/export, saved-rule actions, and validation rollback.
- [x] Refresh README and fixture screenshots after the one-stop implementation.

## Remaining boundaries

- Native window grouping effects, arbitrary geometry expressions, follow-monitor
  policies and the additional effects/focus policies listed above remain future
  extensions.
- Automatic hotplug/profile switching is intentionally not installed. Opt-in
  per-rule placement recovery does run on monitor events, but it never switches
  profiles, fights manual moves, or persists across a full Hyprland restart.
- Local regex inspection is approximate; complete regex-overlap detection is
  not promised.
- Live smoke tests cover an owned window's geometry, not all applications or
  every runtime property. See docs/one-stop-report.md.
- AppSpace parity stops at the bar widget, class verification by launching
  applications, drag-and-drop display arrangement and mutating CLI commands.
  The display keep/revert countdown runs in WindowTuner, so a layout is kept if
  the app is closed before it is confirmed.
