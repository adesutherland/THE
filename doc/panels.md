# Macro panels

This experimental fork supports XEDIT-style forms using SET RESERVED,
SET CTLCHAR and synchronous READ. Run `macro paneldemo` with cREXX or
`macro paneldemo_regina` with Regina in a scratch editing session.

```text
SET CURLINE OFF
SET CTLCHAR ! ESCAPE
SET CTLCHAR p PROTECT WHITE
SET CTLCHAR u NOPROTECT GREEN
SET RESERVED 2 !pName: !uGuest           !p
READ NOCHANGE TAG
```

CURLINE OFF permits reserving every row, including the usual current-line
row. `SET RESERVED * OFF` removes all reserved rows, including an existing
profile's key guide. The examples save `EXTRACT /RESERVED */` and
`EXTRACT /CURSOR/` before opening their forms, then replay the saved reserved
definitions and restore the caller's caret after submission or cancellation.
This keeps the footer and the next typed character in their original places.
PRESERVE/RESTORE saves CURLINE and other editor settings; it does not save
reserved definitions or the cursor. A reusable panel macro must handle those
explicitly, as these examples do.
The examples temporarily hide the ID line, prefix, ruler, command field and
underlying file text. They reserve the form body, declare a dark background
and contrasting editable fields, and restore the caller's file/prefix/command
focus after submission or cancellation. Tab and Shift-Tab cycle the two form
fields. General READ panels still include visible file/prefix/command fields
when the caller enables those areas.
Protection applies during READ; outside READ, FILECTLCHAR continues to be
display markup for ordinary editor operations.

READ preserves the foreground, background and styles declared by CTLCHAR and
reserved lines. Native curses and browser forms use the same field attributes;
the Windows console maps them to its available terminal palette. The browser
keeps Enter, function-key selection and Cancel visible when the form needs
scrolling. CMDLINE OFF omits its command input field during READ.

## Input and results

Syntax: `READ [CMDLINE|ALL|NOCHANGE] [NUMBER] [TAG|NOTAG]`.
CMDLINE and NOTAG are defaults. ALL accepts file changes; NOCHANGE returns
them while retaining the file. Changed fields are returned separately in screen
order, after the key. NUMBER adds file line numbers to file/prefix results.
TAG identifies reserved fields (RES), file fields (FIL), prefix input (PRF),
command input (CMD), Enter (ETK), PF keys (PFK) and PA keys (PAK), following
the documented [IBM READ contract](https://www.ibm.com/docs/en/zvm/7.4.0?topic=macros-read).

Coordinates are one-based logical editor positions: the first file-area row is
row 1; columns include the prefix area when enabled. The protocol/browser model
uses zero-based row/cell values. Terminal title/status lines and browser toolbars
are outside this logical screen. Example results:

```text
PFK 3 exit
RES 2 7 Alice
FIL 11 1 1 replacement text
CMD input for the macro
```

Regina receives a LIFO stack in its default SESSION queue, with the key record
at the front for PULL. A nonempty queue returns RC 8 before input. Named Regina
queues are not selected by this bridge. Both engines also receive READ.0 and
READ.1 through READ.n. cREXX receives these through the ADDRESS sandbox; the
Level G example pushes them in reverse order into its local queue before PULL.
This adaptation is explicit because CREXXSAA has no host queue-insertion API.
A cREXX caller should check its own QUEUED() before READ if it requires the same
nonempty-stack guard.

Tab/Backtab move between editable fields. Arrows, Home, End, Backspace, Delete
and erase-to-end work within fields. Typing overwrites fixed cells and stops at
protected boundaries. Unicode movement uses the editor's text model. Enter,
PF1–PF24 and PA1–PA3 submit without executing their normal editor action; the
key record includes its THE key definition. Clear resets temporary edits.
Escape cancels with RC 4 and READ.0=0. READ from a prefix macro or nested READ
returns RC 4. Invalid operands return RC 5.

ALL stages file replacements before committing. It retains markup bytes and
offscreen text, rejects results exceeding SET TRUNC or the maximum record size,
and leaves every record unchanged on that validation failure. Readonly file
fields are protected. Prefix input is queued for later attention; command input
is returned for the macro to interpret. NOCHANGE and cancellation retain the
file. Reserved input stays temporary until the macro updates its definitions.

## Frontends and limits

The shared field/input model serves curses, the LLM protocol and the browser.
The browser renders editable/protected fields, submission controls and a PF/PA
selector. Outside a panel, function-key guides in reserved rows appear as a
wrapping footer so narrow windows can show every hint. The browser recalculates
the file area's height when the guide wraps. Navigation away releases its
connection, cancelling an active READ; returning reconnects to the editor.
Parser commands use the host PATH: the package provides the cREXX
highlighter; Markdown, Python, C and JavaScript parser executables are optional. Trusted host profiles can bind macros; existing restrictions on
client-issued macro/OS commands remain. A browser disconnect cancels its panel.
Curses uses native keyboard and mouse input. LLM READ holds the macro while
consuming `look`, `text TEXT`, `key NAME`, and `hit ROW CELL`; `cancel`, `quit`
or EOF cancels. Editor commands sent during READ have no effect. Submission
resumes the macro.

This is a form implementation for the fork, not a complete 3270/CMS environment.
CMS MAKEBUF/DROPBUF, XEDIT key BEFORE/AFTER/ONLY/IGNORE policies, programmed
symbols, invisible fields, dynamic resizing during READ, named-stack selection
and full terminal color equivalence are not implemented. FILECTLCHAR panels
require VERIFY column 1. Use spaces for field padding. Limits are 512 fields
and 1024 logical cells per field. Upstream THE and live XEDIT have not been used
for comparison.

Tests cover both engines, stack order, ALL/NOCHANGE, protected boundaries,
Unicode, cancellation, readonly input, prefix results, whole-screen forms and
validation before multi-record commits. Separate tests exercise real curses
and rendered Chromium forms with both engines. Automated Windows release and
installed QA remain distinct from Adrian's manual interactive Windows test,
which still gates handoff to Shmuel. His prefix macros do not require panels.
