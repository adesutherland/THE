# Gemini CLI: THE (The Hessling Editor) SDSLH Integration Guide

## Project Context
THE has been successfully integrated with the **DSL Syntax Highlighter (SDSLH)** platform to provide real-time, out-of-process syntax highlighting and emergency parsing, specifically adopting a CMS XEDIT inspired dark theme.

## Key Architectural Patterns & Integration Points
1. **Thread Safety & Rendering**: THE's screen rendering loop (`show.c`, inside `show_lines`) iterates over the `CodeBufferCharacter` array. This loop MUST be wrapped in `enter_codeblock_critical_section()` and `exit_codeblock_critical_section()` to prevent fatal data races (Signal 11 Segfaults) with the background parser thread that dynamically updates the AST.
2. **Token Colors (Dark Theme)**: A modern dark view is applied via `/usr/local/share/the/profile.the` (and `THE/profile.the`). Native THE syntax color attributes (`ECOLOUR_COMMENTS`, `ECOLOUR_STRINGS`, etc.) are mapped to standard terminal colors (e.g., `set ecolor A cyan on black`) and applied in `show.c` based on `cb_line->characters[i].token_type`.
3. **Diagnostics Rendering**: Syntax errors (`CB_ERROR`) are rendered natively in `show.c` by applying the `A_REVERSE` bitwise attribute. Warnings (`CB_WARNING`) apply `A_UNDERLINE`.
4. **Parser Messages (Status Line)**: THE uses its native `STATOPT` and `EXTRACT` systems to display parser error messages. A new `EXTRACT` variable named `pmsg` (handled by `extract_pmsg` in `query2.c`) securely locks the AST, queries the logical cursor position, and retrieves `node->message` for the character under the cursor. This is surfaced dynamically on the status line via `'set statopt on pmsg.1 23 45 Msg='` in `profile.the`.
5. **Startup & Profile**: The hardcoded `WIDTH.1` statopt was removed from the startup bootstrap (`the.c:Themain`) to prevent `bsearch` collisions and `Error 0001: Invalid operand` crashes during profile execution.

## Current State (As of Last Session)
- [x] **Base Integration**: Complete. `THE` builds with `USE_SDSLH=ON`.
- [x] **Concurrency**: Complete. Critical sections safely wrap the renderer and the `pmsg` extraction logic.
- [x] **Visuals**: Complete. Dark theme active, with token and diagnostic highlighting functioning perfectly.
- [x] **Messages**: Complete. Real-time cursor-tracking error messages populate the status line without hanging or crashing.
- [x] **Testing**: Automated tests moved to standard `tests/` directory. `test_sdslh_integration.sh` correctly executes and passes via CTest.

## Next Steps
- Awaiting user evaluation of the new IDE-like status line messages.
- Potential integration of AST-based code folding (using `TREE_DOWN` / `TREE_UP` markers).
- Continuing to refine any additional `CodeBuffer` library capabilities in the style of the editor.