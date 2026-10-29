#!/usr/bin/env python3
"""Run real GTK callbacks through the real editor, with temporary files only."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary, module, screenshot = map(lambda value: str(Path(value).resolve()), sys.argv[1:4])
    with tempfile.TemporaryDirectory(prefix="the-gtk-runtime-") as directory:
        root = Path(directory)
        (root / "sample.txt").write_text("alpha\nbeta\n", encoding="utf-8")
        (root / "other.txt").write_text("other buffer\n", encoding="utf-8")
        (root / "program.rexx").write_text(
            "say 'GTK program ready'\npull name\nsay 'Hello' name\n", encoding="utf-8")
        environment = dict(os.environ, THE_DRIVER_PATH=module,
                           THE_GTK_PROBE_DIR=directory,
                           THE_GTK_PROBE_SCREENSHOT=screenshot,
                           THE_TOOL_CONFIG_DIR=str(root / "tool-config"))
        completed = subprocess.run([binary, "--driver", "gtk", str(root / "sample.txt")],
                                   cwd=directory, env=environment, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=55)
        if "GTK 3 could not open a display" in completed.stderr + completed.stdout:
            print("SKIP: a GTK display is required; use xvfb-run on Linux")
            return 77
        assert completed.returncode == 0, (completed.returncode, completed.stdout, completed.stderr)
        for marker in ("GTK_OPEN_EDIT_CURSOR_DIRTY_SAVE_REOPEN_PASS",
                       "GTK_DEFAULT_PALETTE_STATUS_RESERVED_TYPE_BACKSPACE_UNDO_SAVE_PASS",
                       "GTK_VIRTUAL_SURFACES_COMMAND_LINE_PASS"):
            assert marker in completed.stdout, (marker, completed.stdout, completed.stderr)
        if environment.get("THE_GTK_REQUIRE_PROGRAM"):
            assert "GTK_PROGRAM_OUTPUT_STDIN_COMPLETION_PASS" in completed.stdout, completed.stdout
            assert "GTK_PROGRAM_INPUT_FOCUS_STATUS_RESERVED_CLICKS_PASS" in completed.stdout, completed.stdout
            assert Path(screenshot).with_name(
                Path(screenshot).stem + "-console-input-focus.png").is_file()
        assert (root / "sample.txt").read_bytes() == "alpha\nbeéta\n".encode("utf-8")
        assert Path(screenshot).is_file(), "GTK widget screenshot was not written"
        for suffix in ("default-launch", "default-wide", "default-narrow",
                       "status-click-typed", "reserved-click-typed"):
            assert Path(screenshot).with_name(
                Path(screenshot).stem + "-" + suffix + ".png").is_file(), suffix
        print(completed.stdout.strip())
        if completed.stderr:
            print(completed.stderr, file=sys.stderr)
        print("GTK runtime screenshot:", screenshot)
    return 0


if __name__ == "__main__":
    sys.exit(main())
