# WindowTuner one-stop implementation

Requested sequence:

1. Safety: review/diff, temporary geometry trial, undo/history, snapshots, dirty guard.
2. Placement: monitor canvas, usable geometry, anchors, percentages, aspect ratio, fallback.
3. Inspector: match testing, matching saved rules, conflicting requested values, coverage.
4. Workflows: custom presets, manually selected rule-collection profiles.
5. Management: application grouping/offline targets, metadata, batch actions, selective imports.
6. Health: version, config paths, errors, capability evidence, redacted diagnostics.
7. Beginner experience: onboarding, basic/advanced, validation, summaries, settings search.

Tests use the public Backend API and the real QML UI with disposable files and
fake hyprctl responses. No test may reload or edit the user's actual configuration.

Safety constraints:

- No claims of complete regex overlap detection or complete external rule inspection.
- Temporary trials change geometry only and retain a revert record before dispatch.
- Percentage placement resolves to logical pixels using current monitor information;
  reopening or resaving is required after monitor geometry changes.
- Profiles switch manually. Monitor-triggered automation is a later enhancement,
  not an implicit background service that changes the desktop.
- Configuration APIs must be identified as verified, unsupported, or unverified.
