# Build and test guide

Configure and build before testing. Never rebuild a tree while its tests run.
Tests use disposable files and libraries; do not use an operator's open file
or installed package as mutable test state.

```sh
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug --parallel
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
```

Integration tests need Bash, ripgrep and Expect. Python drives several
protocol, native-input and packaging checks. Optional parser and browser
requirements are listed in their tests and the [browser test plan](web-driver-test-plan.md).
The standalone [WebAssembly preview](browser-wasm.md) has a separate cross-build
and real-browser persistence/failure harness; native web tests still use the
WebSocket route.
CTest supplies the configured interpreter, runtime, tool and resource paths;
use it instead of assuming a system library or sibling build is selected.

## Coverage by contract

| Contract | Regression route |
| --- | --- |
| Stock profiles, prompts, layout, navigation, Tab insertion and save | `test_default_profile_CREXX`, `test_default_profile_REGINA` |
| Explicit footer names, custom reserved text, order, OFF, replacement and buffer inheritance | `test_named_footer`, `test_screenframe`, `test_llmdriver` |
| Character deletion survives refresh, dirty tracking, save and reopen | `test_delete_persistence_CREXX`, `test_delete_persistence_REGINA` |
| Real terminal Delete/Backspace, text input, Tab, Ctrl-S, Enter and focus restoration | `test_default_profile_curses`, `test_curses_text_entry` |
| Full semantic editor runtime, file ring, UTF edits and modal continuations | `test_the_llm_full_runtime`, `test_the_llm_profile_crexx` |
| Command, prefix and file-area SOS field/query behavior | `test_sos_logical_edit_queries`, `test_sos_navigation_queries`, field/query tests |
| UTF clusters, positions, mutation and rendering metrics | `test_textpos`, `test_textedit`, `test_utfcluster`, `test_utflayout`, `test_utfterm`, `test_utfrepair` |
| No-curses executable/modules and physical ownership | `test_driver_modules`, `test_curses_boundary`, `test_curses_boundary_inventory`, `test_webdriver_no_curses` when enabled |
| Staged CTLCHAR capture, fixed-cell UTF edits, protected bytes, pending records, atomic Apply/undo, close dependency and save/reopen | `test_workbenchfilefields`, `test_workbench_filefields_runtime_CREXX`, `test_workbench_filefields_runtime_REGINA`, `test_workbench_filefields_curses`, `test_web_file_fields_ui`; GTK pane assertions require a working display |
| Native exact-byte FILE stdin, finite EOF, atomic CREATE, collision/policy/storage failure preservation and open-buffer refusal | `test_workbenchfileio`, `test_workbench_fileio_runtime`; unsupported host/mode admission also uses `test_workbench` |
| Owner-bound native source tools, candidate references, argv, pending snapshots and process completion | `test_native_process`, `test_workbench_native_tools_runtime`, `test_workbench_native_tools_curses`, `test_inputevent`, `test_workbench_native_tools_browser`; actual ooRexx uses explicit `THE_TEST_OOREXX_EXECUTABLE`; rebuilt Wasm checks host refusal |
| Typed file actions, no-view startup, owner-polled completion and Program visibility after Build | `test_file_snapshot`, `test_toolhost` |
| New/first input, Unicode multiline selection replacement, pending export, refusal, complete Undo and exact saved bytes | `test_editorclipboard_runtime_CREXX`, `test_editorclipboard_runtime_REGINA`; real Wasm uses `wasm-editing-clipboard.mjs` through the browser smoke harness |
| Current-file GUI settings, profile shortcut authority and no byte/caret/dirty mutation | `test_gui_settings_runtime` |
| Native WebSocket clipboard framing, escaped receipts, large Unicode paste and refusal | `test_native_clipboard_web` when the native web driver is enabled |
| GTK contextual Build/Run/Apply, profile replacement, clipboard targets, font persistence/metrics, sectioned setup and separate readers/input | `test_gtk_input`, `test_gtk_runtime`; the runtime journey requires a working display and a matching native tool bundle |
| Panels, protection, submit/cancel, footer/caret restoration and resumed save | `test_panels`, `test_panel_curses`, browser panel tests |
| Windows console mode before/after quiet shell commands | `test_windows_console_mode_CREXX`, `test_windows_console_mode_REGINA` on Windows |
| Parser synchronization, diagnostics and buffer identity | `test_sdslh_integration`, `test_sdslh_multifile_lifecycle`, `test_the_llm_parser_diagnostics`, web highlighting tests |
| Batch rendering, execution, output validation and source fragments | `test_batch_md_rexx_*` |
| Runtime discovery, cache, package layout and signing procedures | `test_runtime_selection`, installed-package checks and signing-tool tests |
| Snapshot/edit/save behavior under a bounded native stack | `test_llm_stack_budget` |
| Retained tabs, complete status repaint, panel colours and modal input | `test_curses_repaint`, `test_curses_presentation_*`, `test_windows_console_repaint_*` on Windows |
| Release source must already be on published main | `test_release_source` |

The deletion tests first prove that navigation/inspection/save preserve the
original bytes. They then exercise named Delete/Backspace, explicit
`SOS DELCHAR`, `SOS CUADELCHAR`, `SOS DELBACK` and `SOS CUADELBACK`, repeated
deletion, first/last/only characters, no-op boundaries, erase-to-end, CUA joins,
wide characters, combining clusters and a horizontally scrolled record. They
inspect unsaved text, dirty state and cursor, prove inspection
did not write the file, save independently with Ctrl-S and SAVE, and reopen
to check text and bytes. LF is selected explicitly so the byte oracle is
portable. Tab navigation/insertion tests remain separate.

For example, deleting the third character of `alpha` must report `alha` and
dirty=1; `alpha` and dirty=0 indicate a lost edit even if the command returned
success. The shared edit owner is `src/commsos.c`, using `post_process_line`
in `src/util.c` before a repaint can reload `rec`.

## Focused checks and qualification

Functional regression and UI design acceptance have separate outcomes. The
[UI acceptance catalogue](ui-acceptance/catalogue.md) defines versioned goals,
numbered replay steps, fixtures and screenshot checkpoints across browser,
GTK and console. Assess functionality, design fidelity, aesthetics and
discoverability independently against the frozen reference. Existing receipts
are evidence leads, rather than executions of every catalogue case. Keep
`NotRun`, partial coverage, host exceptions and open design decisions explicit;
see the [consolidated gap review](ui-acceptance/gap-review.md).

```sh
ctest --test-dir cmake-build-debug \
  -R 'test_delete_persistence_|test_default_profile|test_sos_logical_edit_queries|test_the_llm_full_runtime' \
  --output-on-failure
```

Run the affected component and public journey before and after a change.
Use the complete suite for candidate qualification. A missing external tool
or platform-only skip is not an executed pass. Preserve baseline failures in
the result, and use [known issues](known-issues.md) for unresolved defects.

Source, packaged/relocated, installed, native input, visible UI and signed
installer checks establish different facts. Record source/dependency versions
and outcomes in the CI/test result, not an accumulating product-document diary.
Physical terminal appearance still needs a native visual check. See
[packaging](packaging.md) for installation and lifecycle tests.

## Common RXBIN tools

With `THE_NATIVE_TOOL_ROOT` configured, CTest includes `test_native_tools` and
`test_tool_console_curses` (the latter requires pyte and a POSIX PTY). Its real
terminal journey checks right/bottom docking and the width threshold,
mouse/keyboard toolbar controls, retained editor Tab bindings, Unicode input
and Backspace, repeated stdin, completion and compact-terminal resizing. The pure
C `test_toolhost` runs without a cREXX SDK. Native browser tests also exercise
the shared console when a tool bundle is available. See
[tools and profile contracts](runtime-tools.md#verification).

For a combined Wasm build with the optional source-profile compiler, the
existing `web/test/wasm-browser-smoke.mjs` also checks actual compiler/assembler
execution, addressed activation, failure retention and IndexedDB restore.
Its fresh-workspace passes check visible syntax and actions for every bundled
language example, runtime errors and Stop/restart, plus the curated public
example inventory. Contract-test RXBIN images stay outside the static site;
the harness reads `tool-example.rxbin` and `source-file-probe.rxbin` from
`build/wasm-review-candidate` by default. Set `THE_WASM_FIXTURE_BUILD` when
testing a different matching build. The candidate target below tests the
extracted distribution ZIP; direct browser runs also support development sites.
`web/test/wasm-interpreter.mjs` qualifies the separately prepared execution VM;
a build of the editor alone does not qualify that VM or its RXBIN image.

## Browser distribution candidate

For a configured combined browser build, use one target to build, package and
qualify the distribution:

```sh
cmake --build build/wasm-review-candidate --target the_wasm_browser_candidate --parallel
```

The target requires committed tracked source and the combined cREXX runtime,
Classic Rexx interpreter/highlighter, source compiler and MiniBASIC capabilities.
It finds Node.js on PATH and Chrome/Chromium in its standard macOS location or
on PATH; set `CHROMIUM_BIN` to select another executable. It creates the
deterministic ZIP in temporary storage, extracts it, serves that copy on an
available loopback port, and runs the complete browser smoke and paint suites.
Test fixtures come from the selected build directory. Focused-suite environment
flags are cleared for this target.

Only after both suites pass does a new immutable directory under
`build/wasm-review-candidate/candidate/` receive
`CRXTHE-browser-candidate.zip`, its `.zip.sha256` checksum, `.qa.json` receipt
and `.qa.log` output. The receipt records the exact source revision, ZIP hash,
capabilities and test commands/results. Failure leaves any earlier successful
candidate unchanged. This qualifies the extracted ZIP locally; publication,
hosted-browser checks and release authorization remain separate.
