"""Exercise launcher boundaries with synthetic credentials; no provider requests."""
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile

launcher = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="pi-launcher-", dir=sys.argv[2]) as tmp:
    root = Path(tmp).resolve()
    agent = root / "pi"
    agent.mkdir()
    config = root / "private settings"
    fake = root / "fake tui"
    fake.write_text("""#!/usr/bin/env python3
import os, sys
from pathlib import Path
assert os.environ['OPENROUTER_API_KEY'] == 'synthetic-test-secret'
assert sys.argv[1:] == ['--config-dir', os.environ['COG_TUI_CONFIG_DIR'], '--mock']
assert sys.stdin.readline() == 'terminal input\\n'
assert 'synthetic-test-secret' not in (Path(sys.argv[2]) / 'settings').read_text()
sys.exit(7)
""")
    fake.chmod(0o700)
    env = dict(os.environ, PI_CODING_AGENT_DIR=str(agent), COG_TUI_BIN=str(fake),
               COG_TUI_CONFIG_DIR=str(config))

    def run(expected):
        result = subprocess.run([str(launcher), "--mock"], env=env,
                                input="terminal input\n", text=True, capture_output=True)
        assert result.returncode == expected, result.stderr
        assert "synthetic-test-secret" not in result.stdout + result.stderr

    settings = agent / "settings.json"
    auth = agent / "auth.json"
    settings.write_text(json.dumps(dict(defaultProvider="openrouter", defaultModel="test/model")))
    auth.write_text(json.dumps(dict(openrouter=dict(type="api_key", key="synthetic-test-secret"))))
    run(7)
    # Installed launcher must find its sibling without depending on PATH.
    installed = root / "relocated prefix" / "commands"
    installed.mkdir(parents=True)
    shutil.copy2(launcher, installed / "cog-tui-pi")
    shutil.copy2(fake, installed / "cog-tui")
    original_launcher = launcher
    launcher = installed / "cog-tui-pi"
    del env["COG_TUI_BIN"]
    run(7)
    launcher = original_launcher
    env["COG_TUI_BIN"] = str(fake)
    assert (config / "settings").stat().st_mode & 0o777 == 0o600
    assert config.stat().st_mode & 0o777 == 0o700
    assert "model=test/model\n" in (config / "settings").read_text()
    env["PI_TEST_KEY"] = "synthetic-test-secret"
    auth.write_text(json.dumps(dict(openrouter=dict(type="api_key", key="PI_TEST_KEY"))))
    run(7)
    before = (config / "settings").read_bytes()
    for value in ["bad\nmodel", "", "a" * 1024]:
        settings.write_text(json.dumps(dict(defaultProvider="openrouter", defaultModel=value)))
        run(2)
        assert (config / "settings").read_bytes() == before
    settings.write_text(json.dumps(dict(defaultProvider="other", defaultModel="test/model")))
    run(2)
    settings.write_text(json.dumps(dict(defaultProvider="openrouter", defaultModel="test/model")))
    auth.write_text('{"openrouter":{"type":"api_key","key":"!do-not-execute"}}')
    run(2)
    auth.write_text('{broken json')
    run(2)
    auth.write_text(json.dumps(dict(openrouter=dict(type="api_key", key="synthetic-test-secret"))))
    config.chmod(0o755)
    run(2)
    config.chmod(0o700)
    (config / "settings").unlink()
    (config / "settings").symlink_to(auth)
    run(2)
print("Pi launcher checks passed")
