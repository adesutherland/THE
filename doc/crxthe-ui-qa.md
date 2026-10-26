# CRXTHE Tab, help, resize and private-kit follow-up — 26 October 2026

Adrian reported Tab disrupting console/browser editing, incomplete browser help
prefixes until refresh, a fixed browser viewport and unwanted bundled cREXX
ports of Shmuel Metz's prefix macros.

## Result

- Stock cREXX and Regina profiles bind Tab to space insertion at eight-column
  stops. The shared SOS INSTAB path uses Unicode-aware text insertion and
  commits its edit buffer before a semantic refresh, preserving typed text,
  dirty tracking and cursor position. Panel Tab remains field navigation.
- Existing tab bytes remain intact and display as the one-cell `→` marker in
  curses and the browser. Their prior native-tab expansion disagreed with the
  editor's cell model. `EXPAND ALL` remains an explicit conversion; the stock
  profile does not silently normalize existing files.
- Frontend-neutral row preparation supplies prefixes before a browser frame
  is published, including the first HELP frame. It avoids drawing/publishing
  recursively and leaves active macro panel rendering to its own presenter.
- ResizeObserver measures the browser editor area and queues bounded resize
  events. The core rebuilds headless windows and populated row state before
  refresh. Snapshots expose total viewport dimensions separately from file-area
  dimensions, accounting for reserved rows. Resizing retains edits/cursor state;
  a panel keeps its declared form dimensions and deferred editor resize is
  applied after it closes.
- All PFXREXX/PFXHLASM implementations and setup macros, including the Regina
  adaptation and cREXX ports, have moved outside public source to the private
  kit. Dedicated workflow/parity tests moved with them. Public package QA now
  exercises editor/runtime functionality using its own tiny probe macro, and
  rejects the private package names as well as patch files. General profiles,
  prompt helpers and panel examples remain.

## Verification

macOS ARM64 full editor regression: **79/79 passed**. The count replaces the
previous 87-test run because eight dedicated prefix-package tests are no longer
part of the public editor suite; they were preserved in the private kit.

Final browser/runtime regressions: **5/5 passed**, covering both macro engines,
viewport grow/shrink, resize after an edit, cursor preservation, first-frame
HELP numbering, real browser Tab/save, retained-tab display, panel form Tab,
resize while a panel is open, submit/cancel and reconnect. First-launch tests
also exercise ASCII, Greek and wide Unicode tab insertion, a new empty file,
retained tab byte preservation, and real curses keyboard input under both
engines. The renderer unit check asserts the one-cell tab marker. TypeScript,
profile generation and diff checks passed.

An LLM-controlled Chrome walkthrough independently confirmed Tab's eight-space
insertion and complete help numbering through the last visible row without
refresh. A local screenshot is retained with Adrian's preview kit.

Local CRXTHE ZIP/PKG staging and relocated installation are checked separately
from source tests. No Windows runner build, signing, publication or upstream
THE/live XEDIT qualification is claimed by this follow-up. The ordinary signed
local installation and historical release assets remain unchanged. Existing
installations may retain old macro files until upgrade cleanup is qualified.
No material has been handed to Shmuel.
