# Omaplace comparison: UX and feature fit for WindowTuner

**Research checked:** 2026-11-24
**Scope:** Omaplace’s first-party repository (`main`, README identifies v0.3.0) compared with the current WindowTuner source and documentation. This is an assessment, not an implementation plan approved for execution.

## Executive summary

**Yes, WindowTuner can adopt useful Omaplace ideas, but it should not copy the plugin or replace its current rule-editor workflow.** The strongest fit is to add a task-focused **Desktops** experience for planning and restoring app layouts, while retaining WindowTuner’s existing live-window rule editor, generated-rule preview, validation, and recovery flow.

There is substantial feature overlap already. WindowTuner has searchable app placement, workspace-to-monitor pins, display layout editing with a keep/revert window, and AppSpace import. The major Omaplace additions are a visual per-desktop plan, named scenes, opening a whole plan at once, and restoring a prior login session. These are meaningful new workflows, not just a visual reskin.

**Recommendation:** adopt Omaplace’s interaction patterns selectively; implement the data and runtime behavior natively in WindowTuner. Start with a read-only desktop overview and explicit named-scene capture/open workflow. Treat automatic session restore, app memory, zone plans, and “update plan when moved” as later opt-in capabilities. Do not have both systems automatically place the same apps at once.

## What Omaplace is

Omaplace is an Omarchy/Hyprland plugin with a Lua module inside Hyprland and a Quickshell service/settings app. Its README describes per-app placement rules, desktop plans and zones, named scenes, session restore, monitor recognition, and app memory. The settings app has Desktops, Apps, Saved, After a restart, and General pages; the CLI exposes scene/rule/session operations. [Omaplace README](https://github.com/felipecpaiva/omaplace/blob/main/README.md#what-it-does) · [settings and CLI](https://github.com/felipecpaiva/omaplace/blob/main/README.md#settings)

It is not simply a UI shell over WindowTuner. Its runtime reacts to window events, captures layouts, starts missing apps, moves existing windows, and tries to restore their places. Its README lists Omarchy 4.5+ with Hyprland Lua and Quickshell as requirements; the settings UI is optional. [Requirements and install](https://github.com/felipecpaiva/omaplace/blob/main/README.md#requirements)

## Feature comparison

| Omaplace idea | WindowTuner today | Fit and recommendation |
|---|---|---|
| Per-app placement rules | AppSpace has searchable app placement rules, workspace, quiet placement, login launch, and startup-only placement. WindowTuner also supports offline app targets. [`README.md:84–114`](../README.md) | **Already substantially covered.** Improve discoverability and explain interactions with saved rules; avoid duplicating another per-app rule editor. |
| Workspace pins and display assignment | AppSpace pins workspaces to displays, marks them always present, and lists assigned apps. [`README.md:95–108`](../README.md) | **Already covered.** Omaplace’s “monitor identity survives port change” is a possible robustness improvement; assess with real monitor data and explicit fallback UI. |
| Display layout editing | WindowTuner edits mode, position, scale, rotation, enabled state, and offers a 15-second keep/revert workflow. [`README.md:104–106`](../README.md); [`appspace.cpp:468–532`](../src/appspace.cpp) | **Already covered, with strong safety.** Keep the timed rollback. Avoid copying Omaplace’s documented mode-toggle workaround, which is limited and may leave mode unchanged at login. |
| Visual plan for each desktop | No current user-facing per-desktop app-and-zone plan is documented; AppSpace is organized around apps, workspace pins, displays, and import. [`README.md:95–114`](../README.md) | **High-value addition.** A Desktops overview with monitor → workspace tiles, current status, and planned apps would make the system easier to understand before editing rules. |
| Zones / tiled layout presets | WindowTuner has a per-window Placement Designer and rule-based geometry; it does not currently offer Omaplace’s desktop-level zone map. | **Possible, but a larger product expansion.** Start with simple two-column / grid plans only after validating how those geometry plans coexist with Hyprland layout modes and Omarchy/omagnet. Do not imply tiling behavior is controlled if only floating geometry is applied. |
| Save/open named scenes | WindowTuner can save and manage rule collections/profiles, but those are rule sets—not snapshots of the apps and positions on a desktop. [`README.md:84–93`](../README.md) | **Strong complementary workflow.** Add an explicit “Save this desktop as a scene” and “Open scene” after defining capture, privacy, duplicate-app matching, and partial-failure behavior. |
| Start missing apps / move open apps to restore a scene | WindowTuner supports login launch on an app rule, but does not describe launching/moving an entire captured set as a single scene operation. [`README.md:99–108`](../README.md) | **Useful, higher risk.** Requires app resolution, safe argument policy, asynchronous run status, timeout/partial results, and a clear preview before launching or moving many windows. |
| Restore previous session | No session snapshot/restore workflow is listed in WindowTuner’s current documented features. | **Potentially valuable, opt-in only.** Default to Ask/Off; make the source session and per-window result visible. Never restore silently until reliability is demonstrated across reboot, display changes, and apps that self-restore. |
| Remember an app’s last location | WindowTuner’s saved rules are explicit; automatic memory would introduce implicit placement. | **Defer.** It can conflict with deliberate class/workspace rules and surprise users. If added, make it a setting with an explicit precedence order and a visible way to forget memory. |
| “Update plan?” after manual move | No equivalent scene-plan learning flow is documented. | **Good optional refinement after scenes exist.** A one-shot notification is preferable to silently rewriting a plan. Needs a reliable way to identify a window’s scene/zone and avoid notification spam. |
| Menu/keyboard access | WindowTuner already installs a normal app-menu desktop entry and has application shortcuts. | Omaplace’s menu integration is explicitly opt-in. WindowTuner does not need to modify Omarchy menu files to gain the main UX benefits. |

## What to borrow from its UX

1. **A desktop-first overview.** Show each connected monitor and its workspaces as a map, with the active layout, app assignments, and whether the tile is a live state or a saved plan. Keep live state and planned state visually distinct.
2. **A clear “remember / open” workflow.** Capture the current workspace into a named scene, then make opening a scene a deliberate user action with a preview of apps to move, launch, or skip.
3. **Useful progress and truthful outcomes.** Omaplace reports per-app placement results and partial failures. WindowTuner should distinguish “placed,” “started,” “not found,” “timed out,” and “skipped”; never reduce a partial restore to a generic success toast.
4. **Explicit recovery choices.** Omaplace uses opt-in restore-at-login modes. WindowTuner should preserve its existing review/rollback ethos and make automatic restore off by default.
5. **Keyboard-oriented navigation.** Page shortcuts, refresh, and help are a good fit for a multi-page desktop tool. Keep existing WindowTuner shortcuts and familiar editor paths intact.

Do **not** copy Omaplace’s five-page structure wholesale. WindowTuner’s core task is to edit and safely apply a rule; its Windows → Layout/Placement/Appearance/Behavior flow and fixed apply path should remain available.

## Architecture and integration risks

- **Native implementation is the better fit.** WindowTuner is a C++/Qt Quick desktop app with a C++ backend and a generated Hyprland Lua module. Omaplace splits work between Lua running in Hyprland and a Quickshell service/QML settings app. Reusing its UI/runtime directly would add a second application/service stack and couple WindowTuner to Quickshell. [WindowTuner stack](../windowtuner.pro); [Omaplace integration design](https://github.com/felipecpaiva/omaplace/blob/main/docs/design.md#architecture)
- **Avoid dual placement owners.** WindowTuner already warns that after AppSpace import the user should disable AppSpace or remove its generated module, because two tools placing the same apps can race. The same ownership rule should apply to Omaplace. Do not run both placement engines against the same app/workspace without an explicit precedence/disable mechanism. [`README.md:108–114`](../README.md)
- **Session restore involves privacy-sensitive data.** Omaplace says it does not store titles or URLs, but it does capture terminal folders and filtered program arguments to relaunch terminals. Any equivalent feature needs a visible capture preview, secret filtering, local-only storage, retention controls, and tests for sensitive arguments. [Omaplace privacy and files](https://github.com/felipecpaiva/omaplace/blob/main/README.md#requirements) · [argument filter source](https://github.com/felipecpaiva/omaplace/blob/main/js/ArgFilter.js)
- **Window identity is imperfect.** Omaplace documents duplicate windows matched by order and browser-profile limitations. WindowTuner should state when it can match only by app class, and offer “place existing / launch missing / skip ambiguous” policies instead of claiming exact restoration. [Omaplace known limits](https://github.com/felipecpaiva/omaplace/blob/main/README.md#known-limits)
- **Monitor fallback must be inspectable.** A plan should say whether it matched the intended monitor, fell back to the internal display, or was skipped. Silent fallback could place a whole scene on the wrong screen.
- **Automatic movement can surprise users.** Scene opening, app memory, and “snap” behavior should be explicit and reversible; do not introduce hidden background movement as part of a visual redesign.

## Suggested implementation sequence

Effort is a rough product/engineering estimate, not a commitment.

### Phase 1 — Desktop overview (small/medium)

- Add a **Desktops** or **Workspace plan** surface with monitor sections and workspace tiles.
- Initially show current workspace, pinned display, assigned apps, and existing rules without adding new automatic behavior.
- Link each tile to the existing AppSpace or rule editor.
- Test multi-monitor layouts, no-monitor data, narrow windows, keyboard navigation, and accessible names.

### Phase 2 — Named scene capture/open (large)

- Capture a workspace’s app classes, monitor/workspace, floating geometry, and ordering; preview the data before save.
- Provide named save, rename, delete, open, and “open this scene on…” actions.
- Start with moving already-open windows and report missing apps; only add launching after safe app-resolution and argument handling are defined.
- Add partial-run state, cancellation where possible, and a clear recovery path.

### Phase 3 — Session restore and optional automation (large/high risk)

- Add event-driven, debounced snapshots with retention and prior-session selection.
- Offer manual restore first; later add Ask/Automatic-at-login with **Off** as default.
- Add “Update plan?” only after scene ownership and movement detection are reliable.
- Consider app memory only after a user-defined precedence model is tested.

## Verdict

**Adopt the desktop-map and scene workflows; do not port the plugin wholesale.** WindowTuner already has much of Omaplace’s per-app placement and display-management foundation. The biggest user benefit would come from turning that foundation into a comprehensible workspace/desktop plan and adding deliberate named scene capture/open. Automatic session restore and implicit app memory can follow only with strong privacy, observability, and recovery safeguards.

The existing architecture and capabilities make this technically plausible, but the scene/session work is a substantial feature expansion—not a cosmetic redesign. The first implementation slice should be the read-only desktop overview, then validate whether users want scene capture before committing to background restore behavior.

## Implementation status

On 2026-11-24, the **Desktops** overview and the first named-scene workflow were implemented:

- The overview summarizes connected monitors, active workspaces, open windows, and saved placement-rule classes. The overview itself is read-only.
- Users can capture the active monitor/workspace, preview exact-class matches, then explicitly confirm applying saved workspace and geometry to matching open windows.
- Scene storage omits titles and window addresses. Missing apps are reported and never launched. Same-class windows are paired in spatial order; scenes require their saved monitor description to be connected.
- Scene removal has a confirmation step. Application is blocked while a geometry/display recovery trial is active, rechecks matches when Apply is pressed, validates all saved geometry before the first side effect, and reports partial failures.
- Event-driven session snapshots and manual restore are implemented as an
  optional feature. Snapshotting is off by default, stores no titles or window
  addresses, debounces Hyprland window/workspace/monitor events, and retains up
  to 10 distinct states. Restore is previewed and explicitly confirmed; missing
  apps are skipped and never launched.
- **Automatic restore at login, launching missing applications, zone plans, and
  app memory remain unimplemented.**

## Sources

- Omaplace repository and README (v0.3.0): https://github.com/felipecpaiva/omaplace
- Omaplace design / integration architecture: https://github.com/felipecpaiva/omaplace/blob/main/docs/design.md
- Omaplace settings app and release notes: https://github.com/felipecpaiva/omaplace/tree/main/app and https://github.com/felipecpaiva/omaplace/blob/main/docs/releases/v0.3.0.md
- WindowTuner capabilities and boundaries: [`README.md`](../README.md), [`TODO.md`](../TODO.md), [`src/AppSpaceDialog.qml`](../src/AppSpaceDialog.qml), [`src/appspace.cpp`](../src/appspace.cpp)
