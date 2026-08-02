# Web Driver POC

Last updated: 2026-08-02.

## Purpose

The web driver proves that THE can support a browser UI without translating a
terminal framebuffer or reimplementing editor behavior in JavaScript. THE
remains the editor and source of truth. The browser renders semantic snapshots
and returns normalized input events.

```text
menu / shortcut / tab / API
            |
      typed action id
            |
 workspace and command policy
            |
 shared action-to-command adapter
            |
      THE command_line()
```

The proof of concept is opt-in with `USE_WEB_DRIVER=ON`. It builds a runtime
module named `the_driver_web`, a production Preact bundle, and copies both into
the normal release layout.

## Components

- `src/drivers/web/webdriver.c` owns a localhost Mongoose HTTP/WebSocket server,
  the connection token, the normalized input queue, and snapshot publication.
- `src/drivers/llm/headlessdriver.c` supplies the complete in-memory
  `TheDriverOps` surface. The web module compiles that implementation without
  its standalone LLM module exports, then overrides input, timeout, and update.
- `src/llm/llmruntime.c`, `src/screenframe.c`, and `src/llm/llmdriver.c` produce
  the semantic screen model already used by the LLM driver.
- `src/inputdispatch.c` is the shared command/prefix/file-area logical-hit
  and typed-action dispatcher used by the regular editor input loop.
- `src/frontendaction.c` is the frontend-independent action registry. It maps
  actions such as `file.save` and `edit.undo` to canonical THE commands such
  as `SAVE` and `SOS UNDO`.
- `src/frontendpolicy.c` canonicalizes paths, enforces writable and read-only
  roots, and restricts raw browser command entry.
- `web/` contains the Preact/TypeScript UI. Vite is build-time tooling only;
  the installed editor does not require a Node.js process.

Mongoose runs in THE's editor thread. `read_input_event()` polls its event loop
while THE waits for input, so editor globals are not accessed concurrently.

## Build And Run

Node.js/npm, a C compiler, CMake, and the normal THE dependencies are required.

```bash
cmake -S . -B cmake-build-web -DUSE_WEB_DRIVER=ON
cmake --build cmake-build-web --target the
./cmake-build-web/release/the --driver web README.md
```

THE prints a URL similar to:

```text
THE web UI: http://127.0.0.1:38127/?token=<random-token>
```

Web mode always loads `web-profile.the`, even if `-n` or `-p` is supplied. The
profile disables implicit OS commands, implicit macros, command synonyms, and
command separators while retaining parser mappings and the web color setup.
`THE_WEB_PROFILE` can select a different trusted, operator-controlled profile.

The session configuration is environment based:

| Variable | Meaning |
| --- | --- |
| `THE_WEB_WORKSPACE` | Primary workspace root; defaults to the current directory. |
| `THE_WEB_WORKSPACE_READONLY` | Makes the primary root read-only when set to `1`, `true`, `yes`, or `on`. |
| `THE_WEB_READONLY_ROOTS` | Additional colon-separated read-only roots on POSIX. |
| `THE_WEB_MACRO_PATH` | Trusted macro search path applied before the web profile. Browser macro execution is not exposed yet. |
| `THE_WEB_BIND` | Listener address; defaults to `127.0.0.1`. |
| `THE_WEB_PORT` | Fixed listener port; defaults to `0` for an OS-assigned port. |
| `THE_WEB_TOKEN` | Proxy-known session token; a random token is generated when omitted. |
| `THE_WEB_ROOT` | Static asset override for development and packaging tests. |

## Current Surface

The UI currently provides:

- semantic file rows, prefixes, row roles, current line, and syntax style runs;
- logical cursor placement and click-to-focus for file and prefix areas;
- Unicode text input and named navigation/editing keys;
- File and Edit menus obtained from the native action registry;
- jailed workspace discovery plus new, open, save, close, and buffer-switch
  actions that execute through THE commands;
- restricted direct THE command submission for navigation, editing, search,
  and selected safe `SET` operands;
- open-buffer tabs, modified state, status text, and parser diagnostics; and
- responsive desktop and narrow-viewport layouts with horizontal editor
  scrolling for the fixed character grid.

The browser never mutates a shadow buffer and does not call `EditFile()`,
`Save()`, or ring functions. Text, keys, logical hits, raw commands, and typed
actions enter THE as `TheInputEvent` values. The native dispatcher converts a
typed action to one canonical command and calls `command_line()`; subsequent
snapshots are rebuilt from the live editor state. Other frontends can reuse
the same registry and input event without adopting the Preact UI.

## Protocol

The WebSocket endpoint is `/ws?token=<token>`. Server messages are either the
existing compact full snapshot or a small control message:

```json
{"type":"hello","protocol":1,"driver":"web","rows":24,"cols":80}
{"type":"actions","actions":[{"id":"file.save","menu":"File","label":"Save","requires_argument":false}]}
{"type":"ack","id":4,"message":"queued"}
{"type":"error","id":4,"message":"invalid input"}
```

Client input messages use these shapes:

```json
{"v":1,"id":1,"type":"snapshot"}
{"v":1,"id":2,"type":"key","key":"left"}
{"v":1,"id":3,"type":"text","text":"hello"}
{"v":1,"id":4,"type":"action","action":"file.save","argument":""}
{"v":1,"id":5,"type":"hit","target":"filearea","line":12,"row":8,"cell":4}
{"v":1,"id":6,"type":"files","root":0,"path":"src"}
```

An acknowledgement means the input was validated and queued. The following
semantic snapshot reports the resulting editor state.

## Security And Deployment Boundary

The intended deployment unit is one THE process in one container per editor
session. A reverse proxy authenticates the user, authorizes the session, and
routes both HTTP and `/ws` to that container. THE does not contain accounts,
cookies, TLS termination, or multi-user session routing. The proxy should use
a dedicated host or preserve the application at `/`; the current bundle uses
same-origin absolute `/assets` and `/ws` paths.

The container is the hard sandbox. The native policy is defense in depth:

- all file paths are canonicalized with `realpath()` and checked against the
  configured roots, including the parent of a new file;
- symlink escapes and `..` escapes are rejected;
- files opened from read-only roots receive THE's forced read-only flag;
- file lifecycle operations are available only as typed actions;
- raw browser commands use an exact built-in allowlist, with OS, Rexx, macro,
  file lifecycle, profile, synonym, command-separator, and implicit execution
  paths unavailable; and
- one token-authenticated WebSocket client controls a process at a time.

For local development, the listener defaults to `127.0.0.1` with a random
token. A container entrypoint can set `THE_WEB_BIND=0.0.0.0`, a fixed port,
and a high-entropy `THE_WEB_TOKEN` known only to the proxy. Static assets do
not contain buffer data. A Content Security Policy and `nosniff` header are
applied to asset responses.

## Verification

`test_the_web_runtime` launches a packaged editor against a temporary jailed
workspace. It authenticates over WebSocket, checks the native action catalog
and file listing, exercises logical hit, restricted command, text, named-key,
Save, and Close paths, and verifies the file. It also proves that raw `EDIT`
cannot open `/etc/passwd`.

When Chrome or Chromium is installed, `test_the_web_browser` drives the real
production UI over the Chrome DevTools Protocol. It checks the native-backed
menus, file explorer, new-file dialog, row focus, browser text composition,
Backspace mapping, Ctrl+S, and File > Close. `test_headlessdriver` guards the
logical-cursor update that makes sequential browser keys reliable, while
`test_webdriver_no_curses` guards the module against curses linkage.

## POC Limits And Next Slices

- The virtual screen remains fixed at 24 by 80. Browser resize negotiation and
  a core-safe virtual resize path should be the next UI slice.
- READV, dialog, and popup transient sessions are not yet represented on the
  WebSocket protocol and should not be invoked from the web driver.
- Snapshots are full-state messages with no revision number or backpressure.
  Add revisioned delta snapshots only after profiling real files.
- The web driver accepts one controlling client. The browser reconnects with
  bounded exponential backoff, but protocol-level revision/resume semantics are
  not yet implemented.
- Trusted macro actions need a separate allowlist layered on
  `THE_WEB_MACRO_PATH`; macros are not currently callable from the browser.
- The built-in raw-command allowlist is compiled in. A future profile command
  can expose operator-approved additions after command metadata includes a
  security classification.
- Directory listings are capped by the protocol response buffer and skip
  symbolic links. Revisioned/paginated listings can be added for large roots.
- Packaging currently installs static files. Embedding the production bundle
  in the module can be evaluated after the UI/protocol boundary stabilizes.
