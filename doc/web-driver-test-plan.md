# Web Driver Test Plan

Last updated: 2026-08-02.

## Objective

Verify that the web driver is a semantic frontend to THE rather than a second
editor implementation. Browser clicks, text, keys, menus, and tabs must enter
the shared input/action layer and produce the same THE command behavior as the
terminal and LLM frontends.

The initial regression focus is prefix-command entry and parser isolation when
switching among files with different syntax types.

## Native Contracts

The expected behavior comes from the existing THE documentation:

- `doc/commands/TEXT.md`: text is applied to the focused command, prefix, or
  file area.
- `doc/commands/ENTER.md`: Enter in the prefix area posts and executes pending
  prefix commands.
- `doc/commands/SET_PREFIX.md` and `doc/commands/SET_PENDING.md`: prefix fields
  are editor-owned line commands, including synonyms and block commands.
- `doc/commands/SET_AUTOCOLOR.md`, `doc/commands/SET_COLORING.md`, and
  `doc/commands/SET_SDSLH.md`: parser selection follows the current file and
  its `AUTOCOLOR` mapping.
- `doc/sdslh.md`: parser completion and style data belong to a full file
  identity; styles from one file must not survive a buffer switch.

## Automated Matrix

| CTest | Coverage | Failure isolated |
| --- | --- | --- |
| `test_frontend_prefix_runtime` | Logical prefix hit, visible `d` entry, Enter, line deletion | Shared cursor/input dispatch or semantic frame |
| `test_the_llm_full_runtime` | Existing logical hits, keyboard editing, Unicode, pending commands | Cross-frontend input regression |
| `test_web_profile_highlighting` | Initial cRexx parser and style spans | Web profile/parser availability |
| `test_web_profile_multifile_highlighting` | cRexx to Markdown to Python and back, with style presence and stale-style absence | Parser/file lifecycle isolation |
| `test_the_web_runtime` | WebSocket hit, text, key, action, save, close, and jail policy | Web protocol/native event bridge |
| `test_the_web_browser` | Production Preact bundle in Chromium, including prefix entry/execution and mixed-language buffer switching | DOM events, keyboard mapping, reconciliation, or CSS style coverage |
| `test_sdslh_multifile_lifecycle` | Lower-level parser selection across multiple files | Core SDSLH lifecycle |

The parser tests return CTest skip code 77 when their external parser binaries
are not installed. A container image intended for web sessions should include
every parser named by `web-profile.the` and treat a skip as a packaging defect.

## Manual Acceptance Check

Build with `USE_WEB_DRIVER=ON`, start THE with a workspace containing `.crexx`,
`.md`, and `.py` files, and open the printed tokenized URL in Chromium.

1. Open a three-line writable text file. Click the prefix field beside line 2,
   type `d`, and confirm the line number changes visibly to `d` while the status
   focus reads `prefix`.
2. Press Enter. Confirm only line 2 is deleted, the buffer becomes modified,
   Undo restores it, and Save clears the modified state.
3. Repeat with `i`, `x`, `"`, `dd`, and a paired `cc` plus target `f`. Confirm
   execution matches terminal THE. Cancel incomplete block commands before the
   next case.
4. Open cRexx, Markdown, and Python files from the explorer. Confirm comments,
   strings, keywords, headings/operators, functions, and identifiers are
   visibly distinct where each language emits them.
5. Switch repeatedly through their buffer tabs. Confirm text never flashes
   with the preceding buffer's classes and parser diagnostics belong only to
   the active file.
6. Edit one file while another parser is completing, then switch away and
   back. Confirm the edit and cursor remain with their file and its highlighting
   converges without changing another buffer.
7. Exercise File New, Open, Save, and Close and Edit Undo from both menu or
   toolbar and their keyboard shortcuts. Confirm each operation has the same
   native result and respects writable/read-only workspace roots.

For a failed manual case, capture the active buffer path, status text, visible
row and prefix, browser console exception, and the server stderr log. Reproduce
the same line/cell with the LLM driver to separate native input behavior from a
browser event or rendering defect.

## Release Gate

The web-driver subset and the full CTest suite must pass. The Chromium test
must run rather than skip on the release/container build host. Run an
AddressSanitizer session through prefix entry, repeated mixed-file switching,
save, and close; no sanitizer report or browser exception is acceptable.
