# Driver operation reference

`src/thedriver.h` defines the public `TheDriverOps` and module lifecycle
interfaces. Keep that header and the implementations as the authority for
signatures. The [cursor/driver contract](cursor-driver-architecture.md) explains
logical versus physical ownership; [architecture](architecture.md) maps the
owning modules.

## Public operations

| Family | Representative operations | Contract |
| --- | --- | --- |
| Opaque surfaces | `create_window`, `delete_window`, `window_size`, `window_origin` | Expose no curses types. Headless drivers retain in-memory surfaces. |
| Explicit cursor | `capture_window_cursor`, `move_window_cursor`, `restore_window_cursor` | Physical compatibility operations; editor intent comes from logical cursor state. |
| Attributes and cells | `set_window_attr`, `set_window_background`, `write_cell_span`, `fill_cells_at`, `overlay_cell_attrs_at` | Accept THE-owned colors/styles and portable cells; terminal palette lowering is driver-private. |
| UTF rendering | `write_render_cells`, `write_render_cluster_at` | Preserve source clusters and logical/display/cursor/repaint metadata. |
| Presentation | `touch_window`, `refresh_window`, `update`, `present_cursor` | Drivers own physical refresh order and cursor painting. |
| Normalized input | `read_input_event` | Return shared text/key/command/hit/action input; raw packets remain inside the input driver. |
| Logical-to-physical cursor | `move_filearea_cursor`, `move_prefix_cursor`, `filearea_cursor_transition` | Materialize shared editor targets without changing text or logical identity. |
| Terminal lifecycle/reporting | shell preparation, background repair and terminal-report operations | Terminal behavior may be a deterministic no-op or log in headless mode. |

The module lifecycle supplies activation, startup/shutdown, suspend/resume,
resize, role/global surface setup and driver diagnostics. The loader validates
`the_driver_module_abi_version` before using a module. Keep the executable and
modules from one build together; incompatible modules are rejected with a
matching-driver/`THE_DRIVER_PATH` diagnostic.

## Shared behavior

`src/driverlayout.c` owns logical/display column conversion, viewport choice
and file-area targets; drivers reuse it. `src/rendercell.c` owns portable
render cells/clusters. `src/inputevent.c` owns normalized input and the
legacy-key adapter. `src/transientui.c` and the panel model own modal state.
These rules must not be duplicated in each frontend.

Curses uses ncurses or PDCurses privately. The LLM driver uses
`src/drivers/llm/headlessdriver.c` for in-memory windows and deterministic
operation logs. The browser reuses that headless implementation and supplies
its transport/input/publication operations. Each operates on the same editor.

The core still has opaque surface and physical compatibility calls. Change
these in bounded behavior-preserving slices, without introducing a parallel
wrapper API. Raw curses headers, packet decoders and attribute encodings must
not cross into editor modules.

## Verification

`test_driver_modules`, `test_curses_boundary` and
`test_curses_boundary_inventory` enforce link and source ownership.
`test_headlessdriver`, `test_uidriver`, `test_virtual_screen` and
`test_llmruntime` cover portable state/presentation. Use full-runtime LLM,
curses PTY and browser tests for a change to editor behavior. The inventory
ratchet detects new debt; operation counts are not a behavior oracle.
