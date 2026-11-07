# WindowTuner implementation checklist

This checklist tracks the requested feature roadmap. Checkboxes are updated as
features land and are verified. A project-local OpenCode TUI plugin at
`.opencode/plugins/windowtuner-tasks/tui.tsx` mirrors this checklist in the
native `sidebar.content` slot and watches the file for updates. See `README.md`
for the user-facing feature set.

## OpenCode progress tracking

- [x] Show the checked/unchecked WindowTuner checklist in OpenCode's native TUI sidebar slot.
- [x] Update the sidebar automatically when `TODO.md` changes.

## Window picker and discovery

- [x] Search current windows by title, class, and workspace.
- [x] Show class, title, workspace, monitor, and floating state.
- [x] Pick a window by clicking it on screen.
- [x] Keep window list current as windows open, close, and rename.
- [x] Show process ID, stable ID, app ID, geometry, fullscreen/maximized state.
- [ ] Group windows from the same application.

## Matching and scope

- [x] Match selected initial class and title safely by default.
- [ ] Configure match fields: class, title, app ID/xdg tag, PID/stable ID, XWayland, role/content/state.
- [x] Choose exact, contains, or regular-expression matching for class/title.
- [x] Add positive and negative match clauses (title exclusion).
- [x] Match by workspace; monitor exclusion still pending.
- [ ] Detect all overlapping rules and show which saved rule matches each window. (Exact duplicates and broad same-class overlaps are flagged conservatively.)

## Layout and placement

- [x] Float/tile/current mode, center, size, position, min/max size.
- [x] Compact, picture-in-picture, and large presets.
- [ ] More presets and user-defined preset save/restore.
- [ ] Edge/corner anchors and margins.
- [ ] Monitor selection, per-monitor geometry, and follow-monitor/workspace.
- [ ] Expression/percentage dimensions and aspect-ratio lock.
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

## Safety, validation, and polish

- [x] Atomic rule writes, first-edit backup, and class/title preview.
- [x] Validate config reload and report Hyprland errors.
- [x] Roll back generated config on failed validation; manual backup/restore controls pending.
- [ ] Dry-run mode and config diff before applying.
- [ ] Verify all features against the installed Hyprland version/capabilities.
- [x] Add regression tests for matching, persistence, import/export, saved-rule actions, and validation rollback.
- [ ] Refresh README and screenshots after feature completion.
