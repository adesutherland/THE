#!/usr/bin/env python3
"""Rewrite curses driver implementation calls to TheDriverOps calls.

The mapping is derived from src/cursesdriver.c:

   .foo = curses_driver_bar

becomes:

   curses_driver_bar(...) -> the_driver->foo(...)

Only C source files under src/ are rewritten. The curses implementation,
curses header, and thedriver.c are intentionally excluded.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
OPS_FILE = REPO_ROOT / "src" / "cursesdriver.c"
SRC_DIR = REPO_ROOT / "src"
EXCLUDED = {
    SRC_DIR / "cursesdriver.c",
    SRC_DIR / "cursesdriver.h",
    SRC_DIR / "thedriver.c",
}


def load_mapping() -> dict[str, str]:
    text = OPS_FILE.read_text()
    match = re.search(
        r"const\s+TheDriverOps\s+the_curses_driver_ops\s*=\s*\{(?P<body>.*?)\n\};",
        text,
        re.S,
    )
    if match is None:
        raise RuntimeError(f"could not find the_curses_driver_ops in {OPS_FILE}")

    mapping: dict[str, str] = {}
    for op, impl in re.findall(
        r"\.(\w+)\s*=\s*(curses_driver_[A-Za-z0-9_]+)\s*,",
        match.group("body"),
        re.S,
    ):
        previous = mapping.setdefault(impl, op)
        if previous != op:
            raise RuntimeError(
                f"{impl} maps to both {previous!r} and {op!r}"
            )
    mapping.update(
        {
            "curses_driver_create_window": "create_window",
            "curses_driver_create_pad": "create_pad",
            "curses_driver_delete_window": "delete_window",
            "curses_driver_enable_keypad": "enable_keypad",
            "curses_driver_capture_window_cursor": "capture_window_cursor",
            "curses_driver_window_origin": "window_origin",
            "curses_driver_window_size": "window_size",
            "curses_driver_replace_current_role_with_relative_window":
                "replace_current_role_with_relative_window",
            "curses_driver_move_window_cursor": "move_window_cursor",
            "curses_driver_restore_window_cursor": "restore_window_cursor",
            "curses_driver_read_window_cell": "read_window_cell",
            "curses_driver_set_window_attr": "set_window_attr",
            "curses_driver_set_window_background": "set_window_background",
            "curses_driver_clear_line_at": "clear_line_at",
            "curses_driver_touch_window": "touch_window",
            "curses_driver_touch_line": "touch_line",
            "curses_driver_refresh_window": "refresh_window",
            "curses_driver_refresh_window_now": "refresh_window_now",
            "curses_driver_refresh_pad": "refresh_pad",
            "curses_driver_draw_box": "draw_box",
            "curses_driver_draw_vertical_line": "draw_vertical_line",
            "curses_driver_add_string_at": "add_string_at",
            "curses_driver_add_chtype_at": "add_cell_at",
            "curses_driver_draw_horizontal_line": "draw_horizontal_line",
            "curses_driver_add_chtype": "add_cell",
            "curses_driver_add_cchar": "add_wide_cell",
            "curses_driver_write_chtype_span": "write_cell_span",
            "curses_driver_write_cchar_span": "write_wide_cell_span",
            "curses_driver_set_cchar_codepoint": "set_wide_cell_codepoint",
            "curses_driver_recolour_cchar": "recolour_wide_cell",
            "curses_driver_write_wide_string_at": "write_wide_string_at",
            "curses_driver_fill_cells_at": "fill_cells_at",
            "curses_driver_write_ascii_cells_at": "write_ascii_cells_at",
            "curses_driver_read_window_key": "read_window_key",
            "curses_driver_read_raw_window_key": "read_raw_window_key",
            "curses_driver_read_mouse_event": "read_mouse_event",
            "curses_driver_force_background_and_refresh":
                "force_background_and_refresh_window",
            "curses_driver_redraw_window": "redraw_window",
            "curses_driver_draw_software_chtype_cell":
                "draw_software_cell",
            "curses_driver_draw_software_blank_cell":
                "draw_software_blank_cell",
        }
    )
    return mapping


def source_files(paths: list[str]) -> list[pathlib.Path]:
    if paths:
        return [pathlib.Path(path).resolve() for path in paths]
    return sorted(SRC_DIR.glob("*.c"))


def rewrite_file(path: pathlib.Path, mapping: dict[str, str]) -> tuple[int, str]:
    if path in EXCLUDED or path.suffix != ".c":
        return 0, path.read_text()

    text = path.read_text()
    total = 0
    for impl, op in sorted(
        mapping.items(), key=lambda item: len(item[0]), reverse=True
    ):
        text, count = re.subn(
            rf"\b{re.escape(impl)}\s*\(",
            f"the_driver->{op}(",
            text,
        )
        total += count
    text, count = rewrite_role_macro_calls(text, set(mapping.values()))
    total += count
    return total, text


ROLE_MACROS = {
    "CURRENT_WINDOW_FILEAREA": "WINDOW_FILEAREA",
    "CURRENT_WINDOW_PREFIX": "WINDOW_PREFIX",
    "CURRENT_WINDOW_GAP": "WINDOW_GAP",
    "CURRENT_WINDOW_COMMAND": "WINDOW_COMMAND",
    "CURRENT_WINDOW_ARROW": "WINDOW_ARROW",
    "CURRENT_WINDOW_IDLINE": "WINDOW_IDLINE",
}

SCREEN_ROLE_PREFIXES = {
    "SCREEN_WINDOW_FILEAREA": "WINDOW_FILEAREA",
    "SCREEN_WINDOW_PREFIX": "WINDOW_PREFIX",
    "SCREEN_WINDOW_GAP": "WINDOW_GAP",
    "SCREEN_WINDOW_COMMAND": "WINDOW_COMMAND",
    "SCREEN_WINDOW_ARROW": "WINDOW_ARROW",
    "SCREEN_WINDOW_IDLINE": "WINDOW_IDLINE",
}

GLOBAL_WINDOWS = {
    "statarea": "THE_DRIVER_GLOBAL_STATAREA",
    "error_window": "THE_DRIVER_GLOBAL_ERROR",
    "divider": "THE_DRIVER_GLOBAL_DIVIDER",
    "filetabs": "THE_DRIVER_GLOBAL_FILETABS",
}


def split_args(arg_text: str) -> list[str]:
    args: list[str] = []
    start = 0
    depth = 0
    quote: str | None = None
    escape = False
    for i, ch in enumerate(arg_text):
        if quote is not None:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote:
                quote = None
            continue
        if ch in ("'", '"'):
            quote = ch
        elif ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            args.append(arg_text[start:i].strip())
            start = i + 1
    tail = arg_text[start:].strip()
    if tail or arg_text.strip():
        args.append(tail)
    return args


def find_call_end(text: str, open_paren: int) -> int | None:
    depth = 0
    quote: str | None = None
    escape = False
    for i in range(open_paren, len(text)):
        ch = text[i]
        if quote is not None:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote:
                quote = None
            continue
        if ch in ("'", '"'):
            quote = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
    return None


def replace_calls(text: str, name: str, transform) -> tuple[str, int]:
    pattern = re.compile(rf"\b{re.escape(name)}\s*\(")
    out: list[str] = []
    pos = 0
    count = 0
    while True:
        match = pattern.search(text, pos)
        if match is None:
            out.append(text[pos:])
            break
        open_paren = match.end() - 1
        close_paren = find_call_end(text, open_paren)
        if close_paren is None:
            out.append(text[pos:])
            break
        args = split_args(text[open_paren + 1 : close_paren])
        replacement = transform(args)
        if replacement is None:
            out.append(text[pos : close_paren + 1])
        else:
            out.append(text[pos : match.start()])
            out.append(replacement)
            count += 1
        pos = close_paren + 1
    return "".join(out), count


def classify_window_expr(expr: str):
    compact = re.sub(r"\s+", "", expr)
    if compact == "CURRENT_WINDOW":
        return ("current_window", ())
    if compact in ROLE_MACROS:
        return ("current_role", (ROLE_MACROS[compact],))
    if compact == "PENDING_WINDOW":
        return ("screen_window", ("pending_screen",))
    if compact in GLOBAL_WINDOWS:
        return ("global", (GLOBAL_WINDOWS[compact],))
    match = re.fullmatch(r"screen\[(.*)\]\.win\[(.*)\]", expr.strip(), re.S)
    if match is not None:
        return ("screen_role", (match.group(1).strip(), match.group(2).strip()))
    match = re.fullmatch(r"SCREEN_WINDOW\s*\((.*)\)", expr.strip(), re.S)
    if match is not None:
        return ("screen_window", (match.group(1).strip(),))
    for macro, role in SCREEN_ROLE_PREFIXES.items():
        match = re.fullmatch(rf"{macro}\s*\((.*)\)", expr.strip(), re.S)
        if match is not None:
            return ("screen_role", (match.group(1).strip(), role))
    return None


def op_call(ops: set[str], op: str, *args: str) -> str | None:
    if op not in ops:
        return None
    return f"the_driver->{op}({', '.join(args)})"


def role_window_call(ops: set[str], args: list[str], table: dict[str, str]):
    if not args:
        return None
    classified = classify_window_expr(args[0])
    if classified is None:
        return None
    kind, values = classified
    op = table.get(kind)
    if op is None:
        return None
    return op_call(ops, op, *values, *args[1:])


def rewrite_role_macro_calls(text: str, ops: set[str]) -> tuple[str, int]:
    total = 0
    transforms = {
        "curses_driver_capture_window_cursor": {
            "current_window": "capture_current_window_cursor",
            "current_role": "capture_current_role_cursor",
            "screen_window": "capture_screen_window_cursor",
            "screen_role": "capture_screen_role_cursor",
            "global": "capture_global_window_cursor",
        },
        "curses_driver_move_window_cursor": {
            "current_window": "move_current_window_cursor",
            "current_role": "move_current_role_cursor",
            "screen_window": "move_screen_window_cursor",
            "screen_role": "move_screen_role_cursor",
            "global": "move_global_window_cursor",
        },
        "curses_driver_restore_window_cursor": {
            "current_window": "restore_current_window_cursor",
            "current_role": "restore_current_role_cursor",
            "screen_window": "restore_screen_window_cursor",
            "screen_role": "restore_screen_role_cursor",
            "global": "restore_global_window_cursor",
        },
        "curses_driver_refresh_window": {
            "current_window": "refresh_current_window",
            "current_role": "refresh_current_role",
            "screen_window": "refresh_screen_window",
            "screen_role": "refresh_screen_role",
            "global": "refresh_global_window",
        },
        "curses_driver_refresh_window_now": {
            "current_window": "refresh_current_window_now",
            "current_role": "refresh_current_role_now",
            "global": "refresh_global_window_now",
        },
        "curses_driver_set_window_attr": {
            "current_window": "set_current_window_attr",
            "current_role": "set_current_role_attr",
            "screen_role": "set_screen_role_attr",
            "global": "set_global_window_attr",
        },
        "curses_driver_touch_window": {
            "current_window": "touch_current_window",
            "current_role": "touch_current_role",
            "screen_role": "touch_screen_role",
            "global": "touch_global_window",
        },
        "curses_driver_window_size": {
            "current_window": "current_window_size",
            "current_role": "current_role_size",
            "screen_role": "screen_role_size",
        },
        "curses_driver_read_window_key": {
            "current_window": "read_current_window_key",
            "current_role": "read_current_role_key",
            "global": "read_global_window_key",
        },
        "curses_driver_read_mouse_event": {
            "current_role": "read_current_role_mouse_event",
        },
        "curses_driver_set_window_timeout": {
            "current_window": "set_current_window_timeout",
        },
        "curses_driver_force_background_and_refresh": {
            "current_window": "force_background_and_refresh_current_window",
        },
        "curses_driver_window_cursor_screen_point": {
            "current_window": "current_window_cursor_screen_point",
        },
        "curses_driver_add_string_at": {
            "global": "add_global_string_at",
        },
    }

    for name, table in transforms.items():
        text, count = replace_calls(
            text, name, lambda args, table=table: role_window_call(ops, args, table)
        )
        total += count

    text, count = replace_calls(
        text,
        "curses_driver_refresh_window",
        lambda args: op_call(ops, "refresh_standard_screen")
        if len(args) == 1 and re.sub(r"\s+", "", args[0]) == "stdscr"
        else None,
    )
    total += count
    text, count = replace_calls(
        text,
        "curses_driver_force_background_and_refresh",
        lambda args: op_call(ops, "force_background_and_refresh_standard_screen")
        if len(args) == 1 and re.sub(r"\s+", "", args[0]) == "stdscr"
        else None,
    )
    total += count
    return text, total


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="*", help="optional files to rewrite")
    parser.add_argument(
        "--check",
        action="store_true",
        help="report files that would change without writing them",
    )
    args = parser.parse_args(argv)

    mapping = load_mapping()
    changed = 0
    replacements = 0
    for path in source_files(args.paths):
        count, rewritten = rewrite_file(path, mapping)
        if count == 0:
            continue
        changed += 1
        replacements += count
        rel = path.relative_to(REPO_ROOT)
        print(f"{rel}: {count}")
        if not args.check:
            path.write_text(rewritten)

    print(
        f"driver-vtable codemod: {replacements} replacements in {changed} files "
        f"from {len(mapping)} ops"
    )
    return 1 if args.check and changed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
