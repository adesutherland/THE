#!/usr/bin/env python3
"""Exercise native workbench controls and console scrollback at two PTY sizes."""
import os
import sys

if os.name == 'nt':
    raise SystemExit(77)
try:
    import pyte
except ImportError:
    raise SystemExit(77)

import fcntl
from pathlib import Path
import pty
import select
import struct
import subprocess
import tempfile
import termios
import time

binary = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parent.parent
tail = b'/* ' + b'a' * 803 + '€'.encode('utf-8')
tail += b'a' * (9000 - len(tail) - len(b' FINAL_UTF8_Z */')) + b' FINAL_UTF8_Z */'
program = ("do i = 1 to 40\n  say 'LINE' i\nend\npull answer\nsay 'ECHO' answer\n" +
           ''.join(f'/* DOC-LINE {line:03d} Ω */\n' for line in range(1, 81)) + tail.decode('utf-8') + '\n')

with tempfile.TemporaryDirectory(prefix='THE workbench sizes ') as temporary:
    root = Path(temporary)
    source = root / 'program.rexx'
    source.write_text(program)
    (root / 'native_replace.the').write_text(
        "/* Replace the focused panel through a real menu-invoked macro. */\n"
        "'workbench begin owner native.logs';\n"
        "if rc <> 0 then exit rc;\n"
        "'workbench panel place native.logone right fallback below,tab mincols 32';\n"
        "if rc <> 0 then exit rc;\n"
        "'workbench commit';\nif rc <> 0 then exit rc;\n'msg LOG_GENERATION_REPLACED';\n")
    # Runtime-owned definitions can be replaced by the later interactive
    # macro. Profile-owned definitions deliberately reject session mutation.
    (root / 'native_logs_setup.the').write_text(
                       "/* Test session LOG graph, before the runner binds routes. */\n"
                       "'workbench begin owner native.logs';\n"
                       "'workbench panel put native.logone kind log model native.stdout';\n"
                       "'workbench panel put native.logtwo kind log model native.stderr';\n"
                       "'workbench panel put native.document kind document buffer active access readonly';\n"
                       "'workbench form put native.person';\n"
                       "'workbench field put native.person native.name label \"Name\" value \"\" access edit maxcells 8';\n"
                       "'workbench field put native.person native.lock label \"Protected\" value \"LOCK Ω\" access readonly maxcells 16';\n"
                       "'workbench field put native.person native.note label \"Note\" value \"kept\" access edit maxcells 12';\n"
                       "'workbench field put native.person native.extra0 label \"Extra 0\" value \"\" access edit maxcells 20';\n"
                       "'workbench field put native.person native.extra1 label \"Extra 1\" value \"\" access edit maxcells 20';\n"
                       "'workbench field put native.person native.extra2 label \"Extra 2\" value \"\" access edit maxcells 20';\n"
                       "'workbench field put native.person native.extra3 label \"Extra 3\" value \"\" access edit maxcells 20';\n"
                       "'workbench field put native.person native.extra4 label \"Extra 4\" value \"\" access edit maxcells 20';\n"
                       "'workbench panel put native.formpane kind fields form native.person';\n"
                       "'workbench route put native.out from tool:classic-rexx/stdout to log:native.stdout';\n"
                       "'workbench route put native.err from tool:classic-rexx/stderr to log:native.stderr';\n"
                       "'workbench menu put native.regression label \"Native regression\" order 999';\n"
                       "'workbench action put native.replace label \"Replace focused LOG\" macro \"native_replace\"';\n"
                       "'workbench item put native.regression native.replaceitem label \"Replace focused LOG\" action native.replace';\n"
                       "'workbench commit';\n"
                       "'msg LOG_GRAPH_READY';\n")
    profile = root / 'profile.the'
    profile.write_text((repo / 'profile.the').read_text() +
                       "\n'define f5 macro native_logs_setup';\n"
                       "'define f2 workbench panel show native.formpane';\n"
                       "'define f11 workbench panel focus native.formpane';\n"
                       "'define f3 workbench panel focus native.document';\n"
                       "'define f4 workbench panel show native.document';\n"
                       "'define f7 workbench panel show native.logone';\n"
                       "'define f8 workbench panel show native.logtwo';\n"
                       "'define f9 workbench panel focus native.logone';\n"
                       "'define c-u cmsg';\n"
                       "'define tab msg EDITOR_TAB';\n'msg PROFILE_READY';\n")
    env = dict(os.environ, TERM='xterm-256color', THE_REXX_INTERPRETER='CREXX',
               THE_HOME_DIR=str(repo), THE_MACRO_PATH=str(root) + os.pathsep + str(repo),
               THE_CREXX_CACHE_DIR=str(root / 'cache'),
               THE_TOOL_CONFIG_DIR=str(root / 'tools'))

    for rows, cols in ((30, 117), (24, 80)):
        master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ,
                    struct.pack('HHHH', rows, cols, 0, 0))
        screen = pyte.Screen(cols, rows)
        stream = pyte.ByteStream(screen)
        output = bytearray()
        process = subprocess.Popen([str(binary), '--driver', 'curses',
                                    '-w', '16384',
                                    '-p', str(profile), str(source)],
                                   stdin=slave, stdout=slave, stderr=slave, env=env)
        os.close(slave)

        def wait(predicate, label, timeout=30):
            deadline = time.monotonic() + timeout
            quiet = time.monotonic()
            while time.monotonic() < deadline:
                if select.select([master], [], [], .05)[0]:
                    try:
                        data = os.read(master, 65536)
                        output.extend(data)
                        stream.feed(data)
                    except OSError:
                        break
                    quiet = time.monotonic()
                if predicate() and time.monotonic() - quiet >= .15:
                    return
            raise AssertionError(f'{cols}x{rows}: {label}; cursor '
                                 f'{screen.cursor.x},{screen.cursor.y}\n' +
                                 '\n'.join(f'{n:02d} {line}' for n, line
                                           in enumerate(screen.display)))

        def send(keys):
            os.write(master, keys)

        def click_at(row, col):
            position = bytes((col + 33, row + 33))
            send(b'\x1b[M ' + position + b'\x1b[M#' + position)

        def has(text):
            return any(text in line for line in screen.display)

        def body():
            if cols == 117:
                return '\n'.join(screen.display[row][73:] for row in range(4, rows - 1))
            return '\n'.join(screen.display[row] for row in range(rows - 4, rows - 1))

        def output_line(number):
            return any(line.strip() == f'LINE {number}' for line in body().splitlines())

        def document_body():
            first = 2 if cols == 117 else rows - 6
            return '\n'.join(line[73:] if cols == 117 else line for line in screen.display[first:rows - 1])

        def console_focused():
            heading = screen.display[0][73:] if cols == 117 else screen.display[rows - 8]
            return 'Esc Cmd' in heading

        def replace_focused_log():
            send(b'\x1b[21~\x1b[F')  # F10, then the final profile-declared action.
            wait(lambda: has('Replace focused LOG'), 'focused LOG opens replacement action')
            send(b'\r')
            wait(lambda: has('LOG_GENERATION_REPLACED') and not has('Replace focused LOG'),
                 'focused LOG generation replacement committed')

        try:
            wait(lambda: 'F10 Menu' in screen.display[rows - 2] and
                 'PROFILE_READY' in screen.display[rows - 4] and
                 'Tools & Profiles' in screen.display[0][:45], 'profile and chrome')
            send(b'\x1b[15~')
            wait(lambda: has('LOG_GRAPH_READY'), 'session LOG graph published before runner launch')
            assert 'MENU (F10)' in screen.display[0][:14], screen.display
            assert 'Console' in screen.display[0][14:25], screen.display
            send(b'\x1b[21~')
            wait(lambda: has('View: Tools & Profiles'), 'F10 menu exposes Run file')
            send(b'\x1b')
            wait(lambda: not has('View: Tools & Profiles'), 'menu closes')
            send(b'\x1b[Z')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (15, 0),
                 'Shift-Tab focuses Console')
            send(b'\x1b[21~')
            wait(lambda: has('View: Tools & Profiles'), 'top chrome F10 opens menu')
            send(b'\x1b')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (15, 0) and
                 not has('View: Tools & Profiles'), 'menu cancel restores top chrome focus')
            send(b'\x1b[Z')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (1, 0),
                 'Shift-Tab focuses Menu')
            send(b'\r' if cols == 117 else b' ')
            wait(lambda: has('View: Tools & Profiles'), 'Menu control opens popup')
            send(b'\x1b')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (1, 0) and
                 not has('View: Tools & Profiles'),
                 'menu cancel restores keyboard selected Menu')
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (15, 0),
                 'Tab returns to Console control')
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (26, 0),
                 'Tab focuses Tools & Profiles')
            if cols == 117:
                click_at(0, 29)
            else:
                send(b'\r')
            wait(lambda: has('Tools & Profiles | Profiles'), 'Tools launcher opens Profiles')
            send(b'\x1b[21~\x1b[H')
            wait(lambda: 'File: New' in screen.display[screen.cursor.y], 'current-file Run menu')
            for _ in range(160):
                line = screen.display[screen.cursor.y]
                if 'Tools: Run' in line:
                    send(b'\r')
                    break
                before = (screen.cursor.y, line)
                send(b'\x1b[B')
                wait(lambda: (screen.cursor.y, screen.display[screen.cursor.y]) != before, 'role action navigation')
            else:
                raise AssertionError('current-file Run missing')
            wait(lambda: 'Program input>' in screen.display[-1] and output_line(40),
                 'role-specific Run starts program and opens console', timeout=40)
            if cols == 117:
                assert 'Program console |' in screen.display[0][73:], screen.display
            else:
                assert 'Program console |' in screen.display[rows - 8], screen.display
            send(b'\x1b')
            wait(lambda: not console_focused() and screen.display[screen.cursor.y].startswith('======>'), 'leave Program input before editor profile shortcuts')
            send(b'\x1b[18~\x1b[19~')  # Show both retained LOG panes from the editor.
            wait(lambda: has('Logs') and not has('Log: native.'), 'SHOW retains selected Program pane')
            click_at(0, 15)
            wait(lambda: not has('Program console |') and not has('Log: native.'), 'Console hides without revealing retained LOG')
            before_focus = len(output)
            click_at(0, 15)
            wait(lambda: console_focused() and 'Program input>' in screen.display[-1],
                 'Console launcher focuses program input')
            assert not screen.cursor.hidden, f'{cols}x{rows}: Program input caret hidden'
            input_start = (73 if cols == 117 else 0) + len('Program input> ')
            assert (screen.cursor.x, screen.cursor.y) == (input_start, rows - 1), screen.cursor
            assert b'\x1b[6 q' in output[before_focus:], \
                f'{cols}x{rows}: profile insert I-beam cursor shape not sent'
            send(b'DRAFT')
            wait(lambda: 'DRAFT' in screen.display[-1] and
                 (screen.cursor.x, screen.cursor.y) == (input_start + 5, rows - 1)
                 and not screen.cursor.hidden, 'visible caret advances with Program input')
            send(b'\x7f')
            wait(lambda: 'DRAF' in screen.display[-1] and
                 'DRAFT' not in screen.display[-1] and
                 (screen.cursor.x, screen.cursor.y) == (input_start + 4, rows - 1)
                 and not screen.cursor.hidden, 'Backspace moves visible Program caret')
            send(b'T')
            wait(lambda: 'DRAFT' in screen.display[-1] and
                 (screen.cursor.x, screen.cursor.y) == (input_start + 5, rows - 1),
                 'Program draft restored after Backspace')
            input_caret = (screen.cursor.x, screen.cursor.y)
            send(b'\t\x1b[F')  # Existing toolbar, then its new Logs control.
            log_control = ((73 if cols == 117 else 0) + 28, 1 if cols == 117 else rows - 7)
            wait(lambda: (screen.cursor.x, screen.cursor.y) == log_control, 'keyboard reaches Logs control')
            send(b'\r')
            wait(lambda: has('Log: native.logone') and has('LINE 40'), 'explicit focus selects routed log')
            send(b'\x1b[H')
            wait(lambda: any((line[73:] if cols == 117 else line).strip() == 'LINE 1' for line in screen.display), 'log Home reaches retained beginning')
            send(b'\x1b[6~')
            wait(lambda: not any((line[73:] if cols == 117 else line).strip() == 'LINE 1' for line in screen.display), 'log PageDown scrolls')
            send(b'\t')
            wait(lambda: has('Log: native.logtwo'), 'Tab selects another named log')
            send(b'h')
            wait(lambda: console_focused() and 'DRAFT' in screen.display[-1]
                 and (screen.cursor.x, screen.cursor.y) == log_control, 'Hide restores Program toolbar and draft')
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == input_caret, 'Program input caret survives LOG focus')
            send(b'\t\x1b[F\r')
            wait(lambda: has('Log: native.logone'), 'remaining log can be focused again')
            send(b'\x1b')
            wait(lambda: console_focused() and (screen.cursor.x, screen.cursor.y) == log_control,
                 'LOG Escape restores Program toolbar without hiding log')
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == input_caret and 'DRAFT' in screen.display[-1],
                 'LOG return preserves unsent Program input')
            send(b'\t\x1b[F\r')
            wait(lambda: has('Log: native.logone'), 'Program origin focuses LOG before replacement')
            replace_focused_log()
            wait(lambda: console_focused() and (screen.cursor.x, screen.cursor.y) == log_control
                 and 'DRAFT' in screen.display[-1], 'generation change restores Program toolbar and draft')
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == input_caret,
                 'generation change preserves Program input caret')
            send(b'\x7f')
            wait(lambda: 'DRAF' in screen.display[-1] and 'DRAFT' not in screen.display[-1]
                 and screen.cursor.x == input_caret[0] - 1,
                 'Program keyboard remains usable after LOG replacement')
            send(b'T')
            wait(lambda: 'DRAFT' in screen.display[-1] and (screen.cursor.x, screen.cursor.y) == input_caret,
                 'Program draft can be restored after LOG replacement')
            send(b'\t\x1b[F\r')
            wait(lambda: has('Log: native.logone') and has('LINE 40'),
                 'replacement LOG retains content and accepts fresh-generation focus')
            send(b'\x1b')
            wait(lambda: console_focused() and (screen.cursor.x, screen.cursor.y) == log_control,
                 'replacement LOG Escape restores Program toolbar')
            send(b'\t')
            wait(lambda: console_focused() and (screen.cursor.x, screen.cursor.y) == input_caret,
                 'replacement LOG Escape restores Program input')
            send(b'\x1b[21~')
            wait(lambda: has('View: Tools & Profiles'), 'Program input F10 opens menu')
            send(b'\x1b')
            wait(lambda: console_focused() and
                 (screen.cursor.x, screen.cursor.y) == input_caret and
                 not has('View: Tools & Profiles'),
                 'menu cancel restores Program input caret and draft')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\t\x1b[C\x1b[C')
            wait(lambda: screen.cursor.y == (1 if cols == 117 else rows - 7),
                 'Program toolbar control selected')
            toolbar_caret = (screen.cursor.x, screen.cursor.y)
            send(b'\x1b[21~')
            wait(lambda: has('View: Tools & Profiles'), 'Program toolbar F10 opens menu')
            send(b'\x1b')
            wait(lambda: console_focused() and
                 (screen.cursor.x, screen.cursor.y) == toolbar_caret and
                 not has('View: Tools & Profiles'),
                 'menu cancel restores selected Program toolbar control')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == input_caret,
                 'Tab returns to Program input')
            send(b'\x1b[H')
            wait(lambda: output_line(1), 'Home reaches first output line')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\x1b[6~')
            wait(lambda: not output_line(1), 'PageDown scrolls output')
            send(b'\x1b[5~')
            wait(lambda: output_line(1), 'PageUp scrolls output')
            send(b'\x1b[F')
            wait(lambda: output_line(40), 'End returns to output tail')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\x1b[17~')  # F6
            wait(lambda: screen.cursor.y < rows - 4 and not console_focused(),
                 'F6 toggles the saved editor command origin back to the file')
            send(b'\x1b')
            wait(lambda: screen.display[screen.cursor.y].startswith('======>'),
                 'Esc reaches the editor command for retained-draft checks')
            assert screen.cursor.hidden, f'{cols}x{rows}: editor software cursor should hide hardware'
            assert screen.buffer[screen.cursor.y][screen.cursor.x].underscore, \
                f'{cols}x{rows}: editor command caret not visually underlined'
            send(b'msg EDITOR_UNSENT\x1b[D\x1b[D')
            wait(lambda: has('msg EDITOR_UNSENT'), 'editor retains an unsent command before LOG focus')
            editor_draft_caret = (screen.cursor.x, screen.cursor.y)
            send(b'\x1b[20~')  # Profile F9 focuses LOG without changing editor origin.
            wait(lambda: has('Log: native.logone'), 'editor origin focuses LOG before replacement')
            replace_focused_log()
            wait(lambda: not console_focused() and (screen.cursor.x, screen.cursor.y) == editor_draft_caret
                 and has('msg EDITOR_UNSENT'), 'generation change restores editor command and caret')
            send(b'\x1b[C')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (editor_draft_caret[0] + 1, editor_draft_caret[1]),
                 'editor keyboard remains usable after LOG replacement')
            send(b'\x1b[D\x1b[20~')
            wait(lambda: has('Log: native.logone') and has('LINE 40'), 'editor can refocus replacement LOG')
            send(b'\x1b')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == editor_draft_caret
                 and has('msg EDITOR_UNSENT'), 'replacement LOG Escape restores editor draft')
            send(b'\x15')
            wait(lambda: not has('msg EDITOR_UNSENT'), 'clear only the unsent editor command')
            send(b'msg DOCUMENT_DRAFT\x1b[D\x1b[D')
            wait(lambda: has('msg DOCUMENT_DRAFT'), 'editor command staged before DOCUMENT SHOW')
            document_origin = (screen.cursor.x, screen.cursor.y)
            send(b'\x1bOS')  # F4 SHOW does not select or focus the document.
            wait(lambda: (screen.cursor.x, screen.cursor.y) == document_origin
                 and not has('Document: native.document'), 'DOCUMENT SHOW preserves editor typing focus')
            send(b'\x1bOR')  # F3 explicit focus.
            wait(lambda: has('Document: native.document') and has('Read-only') and 'do i = 1 to 40' in document_body(),
                 'DOCUMENT focus renders the bound source and read-only cue')
            send(b'POISON\x7f\r')
            wait(lambda: has('Document: native.document') and not has('POISON'), 'DOCUMENT consumes edit and typing keys')
            send(b'\x1b[F')
            wait(lambda: 'FINAL_UTF8_Z' in document_body(), 'DOCUMENT End reaches UTF-8-aligned final bytes')
            send(b'\x1b[H')
            wait(lambda: 'do i = 1 to 40' in document_body() and 'FINAL_UTF8_Z' not in document_body(), 'DOCUMENT Home restores its first page')
            send(b'\x1b[6~')
            wait(lambda: 'do i = 1 to 40' not in document_body(), 'DOCUMENT PageDown advances independent viewport')
            send(b'\x1b')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == document_origin and has('msg DOCUMENT_DRAFT'),
                 'DOCUMENT RETURN preserves unsent editor command and caret')
            send(b'\x15')
            wait(lambda: not has('msg DOCUMENT_DRAFT'), 'clear only the unsent DOCUMENT origin command')
            send(b'query workbench document native.document limit 1 bytes 64\r')
            wait(lambda: has('"section":"document"'), 'physical command-line QUERY DOCUMENT admits its bounded read')
            send(b'msg FORM_DRAFT\x1b[D\x1b[D')
            wait(lambda: has('msg FORM_DRAFT'), 'editor draft staged before named form')
            form_origin = (screen.cursor.x, screen.cursor.y)
            send(b'\x1bOQ')  # F2 SHOW without focus.
            wait(lambda: (screen.cursor.x, screen.cursor.y) == form_origin and not has('Form: native.formpane'),
                 'FORM SHOW keeps editor command focus')
            send(b'\x1b[23~')
            wait(lambda: has('Form: native.formpane') and 'Name' in document_body(), 'FORM FOCUS renders independent fields')
            send('123456789'.encode())
            wait(lambda: '12345678' in document_body() and has('limit-exceeded'), 'FORM cell limit refuses excess burst input')
            send(b'\x15')
            send('hé h'.encode())
            wait(lambda: 'hé h' in document_body() and has('Form: native.formpane'), 'FORM Unicode burst and h stay in editable field')
            send(b'\t\x15draft')
            wait(lambda: 'Note' in document_body() and 'draft' in document_body(), 'FORM Tab skips protected field and scrolls to editable Note')
            send(b'\x1b[Z\x1b[Z\x1b[Z\x1b[Z\r')  # Name, Return, Cancel, Submit.
            wait(lambda: has('Submitted'), 'FORM Submit publishes data without editor execution')
            send(b'\x1b[Ztail')  # Last editable field is reached from Submit.
            wait(lambda: 'Extra 4' in document_body() and 'tail' in document_body(), 'FORM offscreen field reachable with keyboard')
            send(b'\t\t\r')  # Submit, then Cancel.
            wait(lambda: 'tail' not in document_body(), 'FORM Cancel restores submitted baseline')
            send(b'\t\t')  # Return, then Name.
            wait(lambda: 'Name' in document_body() and 'hé h' in document_body(), 'FORM submitted value survives Cancel')
            send(b'\x15Unsent')
            wait(lambda: 'Unsent' in document_body(), 'FORM draft staged before RETURN')
            send(b'\x1b')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == form_origin and has('msg FORM_DRAFT'),
                 'FORM RETURN preserves editor draft and caret')
            send(b'\x15')
            wait(lambda: not has('msg FORM_DRAFT'), 'clear only named form origin command')
            send(b'\x1b[17~')
            wait(lambda: screen.cursor.y < (rows - 8 if cols == 80 else rows - 4),
                 'second F6 reaches editor file')
            click_at(0, 15)
            wait(lambda: console_focused() and 'DRAFT' in screen.display[-1],
                 'F6 leaves unsent input available')
            send(b'\t\x1b')
            wait(lambda: screen.display[screen.cursor.y].startswith('======>'),
                 'toolbar Escape follows command shortcut')
            click_at(0, 15)
            wait(lambda: not has('Program console |') and
                 screen.display[screen.cursor.y].startswith('======>'),
                 'active Console launcher hides its target and keeps editor command focus')
            click_at(0, 15)
            wait(lambda: console_focused() and 'DRAFT' in screen.display[-1],
                 'Escape leaves unsent input available')
            assert (screen.cursor.x, screen.cursor.y) == input_caret and has('Running'), screen.display
            send(b'\t')
            wait(lambda: screen.cursor.y == (1 if cols == 117 else rows - 7),
                 'Tab focuses Program toolbar')
            send(b'\x1b[17~')
            wait(lambda: not console_focused() and
                 screen.cursor.y < (rows - 8 if cols == 80 else rows - 4),
                 'toolbar F6 reaches editor file')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\t')
            wait(lambda: has('EDITOR_TAB'), 'editor Tab retains profile binding')
            file_return_caret = (screen.cursor.x, screen.cursor.y)
            click_at(0, 15)
            wait(lambda: not has('Program console |') and
                 (screen.cursor.x, screen.cursor.y) == file_return_caret,
                 'active Console launcher hides while preserving the editor file caret')
            click_at(0, 15)
            wait(lambda: console_focused() and 'DRAFT' in screen.display[-1] and
                 not screen.cursor.hidden, 'return to visible Program input caret')
            assert (screen.cursor.x, screen.cursor.y) == input_caret and has('Running'), screen.display
            send(b'\t\x1b[F\r')
            wait(lambda: has('Log: native.logone'), 'Program toolbar selects a named pane')
            send(b'\t')
            wait(lambda: has('Document: native.document') and 'do i = 1 to 40' not in document_body(),
                 'DOCUMENT viewport survives pane switching and Program focus origin')
            send(b'\x1b')
            wait(lambda: console_focused() and 'DRAFT' in screen.display[-1], 'DOCUMENT RETURN restores Program draft')
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == input_caret,
                 'DOCUMENT RETURN restores Program input caret')
            send(b'\t\x1b[F\r')
            wait(lambda: has('Log: native.logone'), 'Program reaches named panes after form edits')
            send(b'\t')
            wait(lambda: has('Document: native.document'), 'Program visits DOCUMENT before FORM')
            send(b'\t')
            wait(lambda: has('Form: native.formpane') and 'Unsent' in document_body(), 'FORM retains core-owned draft across Program pane switch')
            send(b'\x1b')
            wait(lambda: console_focused() and 'DRAFT' in screen.display[-1], 'FORM RETURN restores Program draft')
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == input_caret, 'FORM RETURN restores Program input caret')
            send(b'\r')
            wait(lambda: has('ECHO DRAFT') and 'Input closed>' in screen.display[-1]
                 and not screen.cursor.hidden,
                 'submitted stdin completes with visible Program caret')
            assert screen.cursor.y == rows - 1, screen.cursor
            send(b'\x1b')
            wait(lambda: screen.display[screen.cursor.y].startswith('======>'),
                 'Escape returns to THE command after stdin')
            assert screen.cursor.hidden and screen.buffer[screen.cursor.y][screen.cursor.x].underscore, \
                f'{cols}x{rows}: editor command caret not visible after Program return'
            assert source.read_text() == program, 'read-only DOCUMENT changed saved source bytes'
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
            os.close(master)

print('117x30 and 80x24: LOG/DOCUMENT/FORMS, Unicode/protection/limits/Submit/Cancel, Program/editor draft/caret return, menu, scrollback and stdin passed')
