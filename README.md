# WindowTuner

WindowTuner is a Qt Quick desktop application for creating and managing
persistent per-window rules for Hyprland's Lua configuration. Select an open
window, configure its layout and behavior, inspect the generated Lua, then save
and apply the rule.

## What it does

### Discover and select windows

- Lists managed windows and refreshes the list periodically.
- Search by application class, initial/current title, or workspace.
- Pick a window directly from the screen using the crosshair picker.
- Inspect the selected window's class, titles, workspace, monitor, process ID,
  address, geometry, floating/fullscreen state, XWayland status, stable ID,
  XDG tag, and content type.
- Retains the selected window when the list refreshes, when possible.

### Build window rules

Rules use the selected window's initial class and, by default, its initial
title. Class and title matching each support exact, contains, and regular
expression modes. Optional conditions include workspace, XDG tag, XWayland,
fullscreen, grouped state, and content type. A negative title expression can
exclude a title when title matching is disabled. The preview shows the Lua
rule before it is written.

Layout and placement controls include:

- Floating, tiled, or keep-current layout.
- Centering, explicit position, and custom size.
- Minimum and maximum size constraints.
- Workspace assignment and pinning.
- Compact, picture-in-picture, and large-window presets.

Appearance and behavior controls include:

- Active and inactive opacity.
- Border size and corner rounding.
- Blur and shadow overrides.
- Initial focus, animations, dimming, and variable-refresh-rate behavior.
- Persistent floating size.

### Manage saved rules

The saved-rules view supports loading a rule into the editor, updating the
existing rule, duplicating, enabling/disabling, reordering, filtering, applying
to matching open windows, importing/exporting JSON, and confirmed deletion.
Potential overlaps are flagged conservatively for identical rules or broad
same-class rules. This is a warning aid, not a complete proof that two
arbitrary regular expressions overlap.

Rule writes are atomic. Before changing the main Hyprland Lua config, the app
creates `hyprland.lua.windowtuner.bak` if that backup does not already exist.
After changes, it reloads Hyprland and checks `hyprctl configerrors`; failed
validation restores the previous generated rules and config content.

## Requirements

- Linux with a running Hyprland session.
- Hyprland's `hyprctl` available on `PATH`.
- Qt 6 development packages with Quick, QML, and Quick Controls.
- `qmake6`, a C++17 compiler, and `make`.

WindowTuner generates rules using the Hyprland Lua configuration API and
expects the main config at `~/.config/hypr/hyprland.lua`. It adds
`require("hypr.windowtuner")` to that file when needed. If your setup uses a
different Lua entry point or configuration layout, review the generated config
and adjust it before applying rules.

## Build, test, and run

From the repository root:

```sh
./bin/build
./bin/test
./build/windowtuner
```

The test suite covers client parsing, window picking, Lua generation, matching,
rule persistence and management, import/export, and rollback after simulated
Hyprland config validation errors. GUI smoke testing without a display server
can be done with:

```sh
QT_QPA_PLATFORM=offscreen timeout 5s ./build/windowtuner
```

The application is expected to keep running until the timeout; inspect stderr
for QML errors.

## Install

Install the user-local executable, desktop launcher, and icon:

```sh
./bin/install
```

The launcher is installed to `~/.local/share/applications/`, and the executable
to `~/.local/bin/windowtuner`. Ensure `~/.local/bin` is on `PATH`.

Desktop launcher and icon assets are in `pkgbuild/`; a PKGBUILD is not included.

## Configuration and data

WindowTuner stores its rule collection at:

```text
~/.config/hypr/windowtuner.json
```

It compiles enabled rules into:

```text
~/.config/hypr/windowtuner.lua
```

The main Lua config is updated to require the generated module. A one-time
backup is stored beside it as `hyprland.lua.windowtuner.bak`. Back up your
Hyprland configuration before experimenting with window rules.

## OpenCode progress sidebar

This repository includes a project-local OpenCode TUI plugin under
`.opencode/plugins/windowtuner-tasks/`. When OpenCode is launched for this
project, the plugin renders `TODO.md` in the native sidebar and watches the file
for changes. The checklist is the progress source of truth; completed items
are checked there as features are implemented and verified.

## Known limitations

- Monitor-specific placement, window grouping, percentage geometry, additional
  visual effects, undo/history, and dry-run/diff are not implemented yet; see
  [`TODO.md`](TODO.md) for the roadmap.
- Conflict detection is intentionally conservative and does not solve general
  regular-expression overlap.
- Hyprland rule properties and Lua APIs can vary by version. Check
  `hyprctl configerrors` after applying rules and review the generated preview.
- Identical class/title windows cannot be distinguished by a class/title rule;
  use additional match conditions when supported and appropriate.

## License

See [`LICENSE`](LICENSE).
