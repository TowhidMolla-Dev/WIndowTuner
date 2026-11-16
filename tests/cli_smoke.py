#!/usr/bin/env python3
"""Read-only CLI checks. All paths and IPC responses are disposable fixtures."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/windowtuner"
with tempfile.TemporaryDirectory(prefix="windowtuner-cli-", dir="/tmp/opencode") as directory:
    fixture = Path(directory)
    (fixture / "hyprland.lua").write_text("-- CLI fixture\n")
    ctl = fixture / "hyprctl"
    ctl.write_text("""#!/bin/sh
case "$1:$2" in
  -j:clients) echo '[{"address":"0x1","class":"demo","initialClass":"demo","title":"Private fixture","workspace":{"name":"1"}}]' ;;
  -j:monitors) echo '[]' ;;
  -j:version) echo '{"tag":"fixture"}' ;;
  configerrors:) echo '[]' ;;
  eval:*) echo 'ok' ;;
esac
""")
    ctl.chmod(0o700)
    settings = fixture / "settings.json"
    settings.write_text(json.dumps({"layout": "floating", "matchTitle": False, "width": 600, "height": 420}))
    common = ["--config-directory", directory, "--hyprctl", str(ctl), "--json"]
    env = dict(os.environ, QT_QPA_PLATFORM="intentionally-unavailable")

    def invoke(arguments, expected=0):
        result = subprocess.run([str(BINARY), *arguments, *common], env=env, capture_output=True, text=True, timeout=20)
        assert result.returncode == expected, (arguments, result.returncode, result.stderr, result.stdout)
        return result

    assert json.loads(invoke(["check"]).stdout)["sessionReachable"]
    assert json.loads(invoke(["--json", "check"]).stdout)["apiVerified"]
    diagnostics = invoke(["diagnostics"]).stdout
    assert "Private fixture" not in diagnostics and directory not in diagnostics
    assert json.loads(invoke(["inspect"]).stdout)["window"]["class"] == "demo"
    assert json.loads(invoke(["export"]).stdout) == []
    assert json.loads(invoke(["verify", "--settings-file", str(settings)]).stdout)["ok"]
    settings.write_text('{"width": -1}')
    assert not json.loads(invoke(["verify", "--settings-file", str(settings)], 1).stdout)["ok"]
    invoke(["verify"], 1)
    invoke(["unknown-command"], 1)
    assert (fixture / "hyprland.lua").read_text() == "-- CLI fixture\n"
    assert not (fixture / "windowtuner.json").exists()
    assert not (fixture / "windowtuner.lua").exists()
print("CLI smoke: 9 checks passed; no configuration writes.")
