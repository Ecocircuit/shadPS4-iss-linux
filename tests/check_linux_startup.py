#!/usr/bin/env python3
"""Exercise pre-main startup using real fresh processes, not an in-process parser mock."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

executable = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="shadps4-flags-") as directory:
    root = Path(directory)
    binary = root / executable.name
    shutil.copy2(executable, binary)
    (root / "shadps4-flags.txt").write_text(
        "\ufeffSHADPS4_STARTUP_TEST = release value\r\n", encoding="utf-8")
    env = os.environ.copy()
    env.pop("SHADPS4_STARTUP_TEST", None)

    def run(expected):
        env["SHADPS4_STARTUP_EXPECTED"] = expected
        subprocess.run([str(binary), "--gtest_filter=LinuxStartup.*"],
                       env=env, cwd=root, check=True)

    run("release value")
    (root / "shadps4-flags-user.txt").write_text(
        "# literal value, never executed\nSHADPS4_STARTUP_TEST=user $(false) = value\n")
    run("user $(false) = value")
    env["SHADPS4_STARTUP_TEST"] = "inherited"
    run("inherited")
    (root / "defaults/user").mkdir(parents=True)
    (root / "defaults/user/keep.txt").write_text("default")
    run("inherited")
    assert (root / "user/keep.txt").read_text() == "default"
    (root / "user/keep.txt").write_text("custom")
    run("inherited")
    assert (root / "user/keep.txt").read_text() == "custom"
print("PASS: pre-main flags, BOM/CRLF, literal values, precedence, and preserved defaults")
