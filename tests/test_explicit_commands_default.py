#!/usr/bin/env python3
"""Exercise explicit shell and macro commands with implied execution off."""

import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()

with tempfile.TemporaryDirectory(prefix="crxthe-explicit-commands-") as temporary:
    work_dir = Path(temporary)
    (work_dir / "sample.txt").write_text("sample\n", encoding="utf-8")
    (work_dir / "macroprobe.the").write_text(
        "address THE\n'emsg MACRO_PROBE_RAN';\n", encoding="utf-8"
    )
    env = dict(os.environ)
    env["THE_HOME_DIR"] = str(binary.parent / "release")
    env["THE_MACRO_PATH"] = str(work_dir)

    def run(commands):
        result = subprocess.run(
            [str(binary), "--driver", "llm", "-n", "sample.txt"],
            input="\n".join([*commands, "quit", ""]),
            cwd=work_dir,
            env=env,
            text=True,
            encoding="utf-8",
            capture_output=True,
            timeout=30,
            check=True,
        )
        # OS and ! print a terminal prompt before the JSON acknowledgement.
        acks = []
        for line in result.stdout.splitlines():
            start = re.search(r'\{"ok":', line)
            if start:
                acks.append(json.loads(line[start.start():]))
        assert len(acks) == len(commands) + 1, (commands, result.stdout, result.stderr)
        assert acks[-1]["status"] == "bye", (commands, acks)
        return acks[:-1]

    commands = [
        "command query impos",
        "command query impmacro",
        "command echo UNEXPECTED > unknown.txt",
        "command macroprobe",
        "command os echo OS_OK > os.txt",
        "command !echo BANG_OK > bang.txt",
        "command set synonym build osnowait echo SYNONYM_OK > synonym.txt",
        "command build",
        "command macro macroprobe",
        "command set synonym runprobe macro macroprobe",
        "command runprobe",
        "command set impmacro on",
        "command macroprobe",
        "command set impos on",
        "command echo LEGACY_OK > legacy.txt",
    ]
    acks = run(commands)
    assert acks[0]["last_message"].strip().lower() == "impos off", acks[0]
    assert acks[1]["last_message"].strip().lower() == "impmacro off", acks[1]
    assert acks[2]["ok"] == 0 and acks[2]["rc"] == -1, acks[2]
    assert acks[3]["ok"] == 0 and acks[3]["rc"] == -1, acks[3]
    assert not (work_dir / "unknown.txt").exists()
    for index in range(4, len(commands)):
        assert acks[index]["ok"] == 1 and acks[index]["rc"] == 0, (commands[index], acks[index])
    for index in (8, 10, 12):
        assert acks[index]["last_message"] == "MACRO_PROBE_RAN", (commands[index], acks[index])
    for filename, expected in (
        ("os.txt", "OS_OK"),
        ("bang.txt", "BANG_OK"),
        ("synonym.txt", "SYNONYM_OK"),
        ("legacy.txt", "LEGACY_OK"),
    ):
        assert (work_dir / filename).read_text(encoding="utf-8").strip() == expected, filename

    # SET COMPAT reapplies defaults. Exercise every feel and a view opened
    # afterwards so neither implicit execution route is restored.
    for feel in ("the", "xedit", "ispf", "kedit", "keditw"):
        marker = work_dir / f"{feel}-unknown.txt"
        commands = [
            f"command set compat = {feel}",
            "command query impos",
            "command query impmacro",
            f"command echo UNEXPECTED > {marker.name}",
            "command macroprobe",
            f"command edit {feel}-new.txt",
            "command query impos",
            "command query impmacro",
        ]
        acks = run(commands)
        assert acks[0]["rc"] == 0, (feel, acks[0])
        assert acks[1]["last_message"].strip().lower() == "impos off", (feel, acks[1])
        assert acks[2]["last_message"].strip().lower() == "impmacro off", (feel, acks[2])
        assert acks[3]["rc"] == -1 and not marker.exists(), (feel, acks[3])
        assert acks[4]["rc"] == -1, (feel, acks[4])
        assert acks[5]["rc"] == 0, (feel, acks[5])
        assert acks[6]["last_message"].strip().lower() == "impos off", (feel, acks[6])
        assert acks[7]["last_message"].strip().lower() == "impmacro off", (feel, acks[7])

print("Explicit OS/!, MACRO and registered synonym defaults and legacy opt-ins passed")
