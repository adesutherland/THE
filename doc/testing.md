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
CTest supplies the configured interpreter, runtime, tool and resource paths;
use it instead of assuming a system library or sibling build is selected.

## Coverage by contract

| Contract | Regression route |
| --- | --- |
| Stock profiles, prompts, layout, navigation, Tab insertion and save | `test_default_profile_CREXX`, `test_default_profile_REGINA` |
| Character deletion survives refresh, dirty tracking, save and reopen | `test_delete_persistence_CREXX`, `test_delete_persistence_REGINA` |
| Real terminal Delete/Backspace, text input, Tab, Ctrl-S, Enter and focus restoration | `test_default_profile_curses`, `test_curses_text_entry` |
| Full semantic editor runtime, file ring, UTF edits and modal continuations | `test_the_llm_full_runtime`, `test_the_llm_profile_crexx` |
| Command, prefix and file-area SOS field/query behavior | `test_sos_logical_edit_queries`, `test_sos_navigation_queries`, field/query tests |
| UTF clusters, positions, mutation and rendering metrics | `test_textpos`, `test_textedit`, `test_utfcluster`, `test_utflayout`, `test_utfterm`, `test_utfrepair` |
| No-curses executable/modules and physical ownership | `test_driver_modules`, `test_curses_boundary`, `test_curses_boundary_inventory`, `test_webdriver_no_curses` when enabled |
| Panels, protection, submit/cancel, footer/caret restoration and resumed save | `test_panels`, `test_panel_curses`, browser panel tests |
| Windows console mode before/after quiet shell commands | `test_windows_console_mode_CREXX`, `test_windows_console_mode_REGINA` on Windows |
| Parser synchronization, diagnostics and buffer identity | `test_sdslh_integration`, `test_sdslh_multifile_lifecycle`, `test_the_llm_parser_diagnostics`, web highlighting tests |
| Batch rendering, execution, output validation and source fragments | `test_batch_md_rexx_*` |
| Runtime discovery, cache, package layout and signing procedures | `test_runtime_selection`, installed-package checks and signing-tool tests |

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
