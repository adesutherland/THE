#!/usr/bin/env python3
"""Real native parser: severity projection, cursor PMSG and read-only refresh."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parent.parent
rxc = os.environ.get("THE_CREXX_RXC") or shutil.which("rxc")
if not rxc:
    candidate = repo.parent / "CREXX/cmake-build-debug/bin/rxc"
    if candidate.is_file():
        rxc = str(candidate)
if not rxc or not Path(rxc).is_file():
    print("Skipping native parser presentation; rxc is unavailable", file=sys.stderr)
    raise SystemExit(77)


def run_case(root, source, line, cell, point):
    sample = root / "error.crexx"
    clean = root / "clean.crexx"
    sample.write_text(source)
    clean.write_text("say 'ok'\n")
    original = sample.read_bytes()
    commands = [
        f"command set sdslh rxc {rxc} --syntaxhighlight",
        "command set autocolor *.crexx rxc", "command coloring on auto",
        "command set statusline bottom", "command set statopt off *",
        "command set statopt on pmsg.1 15 38", "command set clock on",
        "command set hexdisplay on", "command set color cberror white on firebrick",
        "command set color pmsgerror white on firebrick", "command sdslhwait 5000",
        "focus filearea", f"hit filearea {line} 1 {cell}",
        "look full compact max=120",
        "command cursor goto 1 1", "look full compact max=120",
        f'command edit "{clean}"', "command sdslhwait 5000",
        "command cursor goto 1 1", "look full compact max=120",
        f'command edit "{sample}"', "command sdslhwait 5000",
        f"command cursor goto {line} {cell + 1}", "look full compact max=120",
        "command replace say 'fixed'", "command sdslhwait 5000",
        "look full compact max=120", "quit",
    ]
    env = dict(os.environ, TERM="", THE_REXX_INTERPRETER="CREXX",
               THE_HOME_DIR=str(binary.parent / "release"),
               THE_DRIVER_PATH=str(binary.parent), THE_CREXX_CACHE_DIR=str(root / "cache"),
               THE_TOOL_CONFIG_DIR=str(root / "tools"))
    completed = subprocess.run([str(binary), "--driver", "llm", "-n", str(sample)],
                               input="\n".join(commands) + "\n", text=True,
                               capture_output=True, env=env, timeout=30)
    assert completed.returncode == 0, (completed.stdout, completed.stderr)
    snapshots = []
    for output in completed.stdout.splitlines():
        try:
            value = json.loads(output)
        except ValueError:
            continue
        if "screen_rows" in value:
            snapshots.append(value)
    assert len(snapshots) == 5, completed.stdout
    error, away, other, returned, repaired = snapshots
    for snapshot in (error, returned):
        assert snapshot["buffer"]["dirty"] == 0, snapshot
        assert snapshot["focus"]["line"] == line and snapshot["focus"]["cell"] == cell, snapshot
        message = snapshot["status_pmsg"]
        assert message and message["severity"] == "ERROR" and "THEN" in message["text"], snapshot
        assert "Copyright" not in snapshot["status_line"], "startup text masked the parser message before the first key"
        assert snapshot["status_line"][message["start"]:message["start"] + message["length"]] == message["text"][:message["length"]]
        assert message["colour"]["background"] == "#B22222", message
        assert snapshot["diagnostic_colours"]["error"]["background"] == "#B22222"
        row = next(row for row in snapshot["screen_rows"] if row["line"] == line and row["role"] == "file")
        assert [row["t"] for row in snapshot["screen_rows"] if row["role"] == "file"] == source.splitlines(), snapshot
        assert any(start == point and length == 4 and style == "diagnostic-error"
                   for start, length, style in row.get("s", [])), row
        assert all(a[0] + a[1] <= b[0] for a, b in zip(row["s"], row["s"][1:])), row
    assert away["status_pmsg"] is None, away
    for snapshot in (other, repaired):
        assert snapshot["status_pmsg"] is None, snapshot
        assert not snapshot.get("diagnostics"), snapshot
        assert all(not style.startswith("diagnostic-") for row in snapshot["screen_rows"]
                   for _, _, style in row.get("s", [])), snapshot
    assert repaired["buffer"]["dirty"] == 1, repaired
    assert sample.read_bytes() == original, "diagnostic display or repair saved the source"
    assert clean.read_text() == "say 'ok'\n"


with tempfile.TemporaryDirectory(prefix="THE parser presentation ") as temporary:
    root = Path(temporary)
    run_case(root, 'say "hi"\nif then\n', 2, 3, 3)
    # The wide character precedes the error on the same line: display cells,
    # parser slots and semantic style offsets must not be confused.
    run_case(root, 'say "界"; if then\n', 1, 13, 12)
print("Parser presentation passed: real errors, Unicode, cursor leave, file switch, repair and unchanged saved bytes")
