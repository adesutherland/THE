# Gemini CLI: THE SDSLH Integration Notes

## Project Context
THE integrates with the **DSL Syntax Highlighter (SDSLH)** platform to provide
syntax/style spans and parser diagnostics. The current architecture exposes
that state to both the curses driver and the no-curses `the --driver llm`
surface.

## Key Architectural Patterns & Integration Points

1. **Thread Safety**: CodeBuffer reads must be protected with the SDSLH
   critical-section helpers because the parser updates state asynchronously.
2. **Logical Styles**: Parser token categories should become THE logical
   syntax/style spans. Curses lowering to terminal colours and attributes
   belongs in `src/drivers/curses/**`; LLM snapshots should report semantic
   style names.
3. **Diagnostics**: Parser messages are editor state. `EXTRACT /PMSGS/` and
   LLM snapshot `diagnostics` must report parser messages without relying on
   terminal colour or physical cursor placement.
4. **Profiles**: `profile.the` / `profile_crexx.the` remain the normal place
   for runtime parser and status-line setup. `the --driver llm` should expose
   the resulting real runtime state through snapshots.

## Current State (As of Last Session)

- [x] **Base Integration**: `THE` builds with `USE_SDSLH=ON`.
- [x] **Concurrency**: critical sections protect parser-backed renderer and
  diagnostics reads.
- [x] **Styles**: runtime highlighting surfaces as logical style spans and is
  covered by full-runtime LLM tests.
- [x] **Diagnostics**: parser messages are available through `EXTRACT /PMSGS/`
  and first-class LLM snapshot diagnostics.
- [x] **Testing**: SDSLH integration tests live in `tests/` and run through
  CTest with skip-safe behavior where external parser fixtures are unavailable.

## Next Steps

- Broaden parser/language fixtures where useful for `the --driver llm`.
- Consider AST-backed editing features such as folding only after the driver
  boundary remains stable on macOS, Linux, and Windows.
