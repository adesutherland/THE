#!/usr/bin/env python3
"""Rewrite safe curses-facing editor types to neutral driver types.

This is intentionally narrower than a whole-tree ``chtype`` sweep.  It handles
storage and helper signatures that already flow through ``TheDriverOps``:

* editor window handles become opaque ``TheDriverWindow *``.
* renderer/colour attributes become ``TheDriverAttr``.
* narrow renderer cells and line buffers become ``TheDriverCell``.
* wide renderer cells become ``TheDriverWideCell``.

The curses implementation remains responsible for casting these neutral values
to curses types at the physical edge.
"""

from __future__ import annotations

import argparse
import pathlib
import re


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]


REPLACEMENTS: dict[str, list[tuple[str, str]]] = {
    "src/the.h": [
        (r"\bchtype\s+mod;", "TheDriverAttr mod;"),
        (r"\bchtype\s+mono;", "TheDriverAttr mono;"),
        (r"\bchtype\s+\*highlighting;", "TheDriverAttr *highlighting;"),
        (r"\bchtype\s+normal_colour;", "TheDriverAttr normal_colour;"),
        (r"\bchtype\s+other_colour;", "TheDriverAttr other_colour;"),
        (r"\bchtype\s+prefix_colour;", "TheDriverAttr prefix_colour;"),
        (r"\bchtype\s+gap_colour;", "TheDriverAttr gap_colour;"),
        (
            r"\bchtype\s+prefix_highlighting\[MAX_PREFIX_WIDTH\+1\];",
            "TheDriverAttr prefix_highlighting[MAX_PREFIX_WIDTH+1];",
        ),
        (
            r"\bchtype\s+gap_highlighting\[MAX_PREFIX_WIDTH\+1\];",
            "TheDriverAttr gap_highlighting[MAX_PREFIX_WIDTH+1];",
        ),
        (
            r"\bchtype\s+highlighting\[THE_MAX_SCREEN_WIDTH\];",
            "TheDriverAttr highlighting[THE_MAX_SCREEN_WIDTH];",
        ),
        (
            r"\bWINDOW\s+\*win\[VIEW_WINDOWS\];",
            "TheDriverWindow *win[VIEW_WINDOWS];",
        ),
        (r"\bchtype\s+fore;", "TheDriverAttr fore;"),
        (r"\bchtype\s+back;", "TheDriverAttr back;"),
    ],
    "src/vars.h": [
        (r"\bextern\s+WINDOW\s+\*statarea,", "extern TheDriverWindow *statarea,"),
        (r"\bextern\s+chtype\s+etmode_table\[256\];", "extern TheDriverCell etmode_table[256];"),
        (r"\bextern\s+cchar_t\s+\*linebufch;", "extern TheDriverWideCell *linebufch;"),
        (r"\bextern\s+chtype\s+\*linebufch;", "extern TheDriverCell *linebufch;"),
    ],
    "src/the.c": [
        (
            r"\bWINDOW\s+\*statarea=NULL,\*error_window=NULL,\*divider=NULL,\*filetabs=NULL;",
            "TheDriverWindow *statarea=NULL,*error_window=NULL,*divider=NULL,*filetabs=NULL;",
        ),
        (r"\bchtype\s+_THE_FAR\s+etmode_table\[256\];", "TheDriverCell _THE_FAR etmode_table[256];"),
        (r"\bcchar_t\s+\*linebufch;", "TheDriverWideCell *linebufch;"),
        (r"\bchtype\s+\*linebufch;", "TheDriverCell *linebufch;"),
        (
            r"\(cchar_t\s+\*\)\(\*the_malloc\)\(linebuf_size \* sizeof\(cchar_t\)\)",
            "(TheDriverWideCell *)(*the_malloc)(linebuf_size * sizeof(TheDriverWideCell))",
        ),
        (
            r"\(chtype\s+\*\)\(\*the_malloc\)\(linebuf_size \* sizeof\(chtype\)\)",
            "(TheDriverCell *)(*the_malloc)(linebuf_size * sizeof(TheDriverCell))",
        ),
    ],
    "src/proto.h": [
        (r"\breadv_cmdline \(CHARTYPE \*, WINDOW \*, int\)", "readv_cmdline (CHARTYPE *, TheDriverWindow *, int)"),
        (r"\bchtype \*apply_ctlchar_to_reserved_line \(RESERVED \*\)", "TheDriverAttr *apply_ctlchar_to_reserved_line (RESERVED *)"),
        (r"\bmy_getch  \(WINDOW \*\)", "my_getch  (TheDriverWindow *)"),
        (r"\bredraw_window \(WINDOW \*\)", "redraw_window (TheDriverWindow *)"),
        (
            r"\bput_string \(WINDOW \*, ROWTYPE, COLTYPE, CHARTYPE \*, LENGTHTYPE\)",
            "put_string (TheDriverWindow *, ROWTYPE, COLTYPE, CHARTYPE *, LENGTHTYPE)",
        ),
        (r"\bput_char \(WINDOW \*, chtype, CHARTYPE\)", "put_char (TheDriverWindow *, TheDriverCell, CHARTYPE)"),
        (r"\bmy_wmove \(WINDOW \*, short, short, short, short\)", "my_wmove (TheDriverWindow *, short, short, short, short)"),
        (
            r"\badjust_window \(WINDOW \*,short ,short ,short ,short \)",
            "adjust_window (TheDriverWindow *,short ,short ,short ,short )",
        ),
        (r"\bmy_wclrtoeol \(WINDOW \*\)", "my_wclrtoeol (TheDriverWindow *)"),
        (r"\bmy_wdelch \(WINDOW \*\)", "my_wdelch (TheDriverWindow *)"),
        (r"\bchtype merge_curline_colour", "TheDriverAttr merge_curline_colour"),
    ],
    "src/commutil.c": [
        (r"\breadv_cmdline\(CHARTYPE \*initial, WINDOW \*dw, int start_col\)", "readv_cmdline(CHARTYPE *initial, TheDriverWindow *dw, int start_col)"),
    ],
    "src/drivers/curses/getch.c": [
        (r"\bmouse_getch_trace\(WINDOW far \*winptr, int key\)", "mouse_getch_trace(TheDriverWindow *winptr, int key)"),
        (r"\bmouse_getch_trace\(WINDOW \*winptr, int key\)", "mouse_getch_trace(TheDriverWindow *winptr, int key)"),
        (r"\bmy_getch \(WINDOW far \*winptr\)", "my_getch (TheDriverWindow *winptr)"),
        (r"\bmy_getch \(WINDOW \*winptr\)", "my_getch (TheDriverWindow *winptr)"),
    ],
    "src/execute.c": [
        (r"\bWINDOW \*dialog_win=NULL;", "TheDriverWindow *dialog_win=NULL;"),
        (r"\bWINDOW \*pad;", "TheDriverWindow *pad;"),
    ],
    "src/util.c": [
        (r"\bvoid put_string\( WINDOW \*win,", "void put_string( TheDriverWindow *win,"),
        (r"\bvoid put_char\(WINDOW \*win,chtype ch,", "void put_char(TheDriverWindow *win,TheDriverCell ch,"),
        (r"\bWINDOW \*adjust_window\(WINDOW \*win,", "TheDriverWindow *adjust_window(TheDriverWindow *win,"),
        (r"\bWINDOW \*neww=NULL;", "TheDriverWindow *neww=NULL;"),
        (r"\bshort my_wclrtoeol\(WINDOW \*win\)", "short my_wclrtoeol(TheDriverWindow *win)"),
        (r"\bshort my_wdelch\(WINDOW \*win\)", "short my_wdelch(TheDriverWindow *win)"),
        (r"\bshort my_wmove\(WINDOW \*win,", "short my_wmove(TheDriverWindow *win,"),
        (r"\bchtype filetab_cell=0;", "TheDriverCell filetab_cell=0;"),
    ],
    "src/show.c": [
        (r"\bWINDOW \*_fast_win;", "TheDriverWindow *_fast_win;"),
        (r"\bstatic WINDOW \*_fast_win;", "static TheDriverWindow *_fast_win;"),
        (r"\bcchar_t \*dest;", "TheDriverWideCell *dest;"),
        (r"\bchtype \*dest,\*highl;", "TheDriverCell *dest,*highl;"),
        (r"\bchtype \*dest,C =", "TheDriverCell *dest,C ="),
        (r"\bchtype \*highl", "TheDriverAttr *highl"),
        (
            r"\(cchar_t \*\)\(\*the_realloc\)\(linebufch, \(linebuf_size \* sizeof\(cchar_t\)\)\)",
            "(TheDriverWideCell *)(*the_realloc)(linebufch, (linebuf_size * sizeof(TheDriverWideCell)))",
        ),
        (
            r"\(chtype \*\)\(\*the_realloc\)\(linebufch, \(linebuf_size \* sizeof\(chtype\)\)\)",
            "(TheDriverCell *)(*the_realloc)(linebufch, (linebuf_size * sizeof(TheDriverCell)))",
        ),
    ],
    "src/reserved.c": [
        (
            r"\(chtype \*\)\(\*the_malloc\)\( \( strlen\( \(DEFCHAR \*\)templine \) \+ 1 \) \* sizeof\(chtype\) \)",
            "(TheDriverAttr *)(*the_malloc)( ( strlen( (DEFCHAR *)templine ) + 1 ) * sizeof(TheDriverAttr) )",
        ),
    ],
}


def rewrite_file(path: pathlib.Path) -> int:
    rel = path.relative_to(REPO_ROOT).as_posix()
    replacements = REPLACEMENTS.get(rel, [])
    if not replacements:
        return 0
    text = path.read_text()
    total = 0
    for pattern, replacement in replacements:
        text, count = re.subn(pattern, replacement, text)
        total += count
    if total:
        path.write_text(text)
    return total


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", help="optional paths to rewrite")
    args = parser.parse_args()

    paths = [pathlib.Path(path).resolve() for path in args.paths]
    if not paths:
        paths = [(REPO_ROOT / rel) for rel in REPLACEMENTS]

    total = 0
    for path in paths:
        count = rewrite_file(path)
        if count:
            print(f"{path.relative_to(REPO_ROOT)}: {count}")
            total += count
    print(f"total replacements: {total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
