# Explicit native source tools

The bounded native-process adapter runs captured source through an explicitly
configured executable. Its current execution qualification is macOS arm64.
Linux, Windows and browser/Wasm return `unsupported-host`; restricted native
browser sessions return `policy-denied`. Their compiled subprocess components
do not establish runtime qualification. No ooRexx executable is installed,
discovered through PATH, selected automatically or substituted with Classic Rexx.

## Declare, inspect and run

```text
WORKBENCH BEGIN OWNER public.tools
WORKBENCH TOOL PUT public.oorexx ROLE RUNNER ADAPTER NATIVE-PROCESS EXECUTABLE "/explicit/ooRexx/bin/rexx" SOURCE SNAPSHOT
WORKBENCH ACTION PUT public.run LABEL "Run with ooRexx" TOOL public.oorexx SOURCE ACTIVE ARG "Mixed Case" ARG ""
WORKBENCH MENU PUT public.tools LABEL "Tools" ORDER 50
WORKBENCH ITEM PUT public.tools public.runitem LABEL "Run with ooRexx" ACTION public.run
WORKBENCH COMMIT
QUERY WORKBENCH TOOLS
QUERY WORKBENCH CAPABILITIES
WORKBENCH RUN public.oorexx SOURCE ACTIVE ARG "Mixed Case" ARG ""
```

Selecting the menu item or issuing `WORKBENCH INVOKE public.run` explicitly
executes the stored TOOL action. Definition, profile import/activation, COMMIT,
inspection and showing/focusing a pane do not execute it. A missing executable
is an inspectable unavailable dependency (`capability-unavailable`), rather than
a reason to execute another interpreter. Executable availability is checked
again at invocation. The adapter can also run another explicitly configured
source interpreter with the same snapshot-first process argv contract.

Definitions remain in the workbench graph, separate from the legacy RX tool
catalog and its persisted registry. `RX REGISTER` retains its RXBIN meaning;
changing the macro interpreter does not choose this runner. Native source tools
also work when the macro engine is NONE and the optional RXBIN helper is absent.

Tool/action/route references resolve together against the complete candidate
graph. A tool belongs to its transaction owner; another owner cannot reference
or replace it. Removing a referenced tool refuses the complete publication.
Remove/rebind its dependent actions and routes in the same transaction, or use
`RESET OWNER` for the complete owned graph. A late outer-profile failure retains
the previous published graph. `TOOL REMOVE id` removes an owned definition.
Neither removing definitions nor hiding panes stops a running job: it retains
copied settings and route bindings, and the existing Program console Stop/EOF
controls remain available.

## Snapshot, argv and completion boundary

The editor captures the active buffer and pending working record at a safe
execution boundary, without Save. It stages those immutable captured bytes in
a private temporary source file and passes its pathname before the separate
argv entries. Logical records use LF, as in the existing source-capture service;
source containing NUL refuses. Closing/switching the intended source before a
queued menu action dispatches returns `stale-revision`, without capturing the
new current buffer. The queue pins an immutable document ID, rather than a
reusable file pointer or filename. Source bytes are captured only at dispatch.
Invocation during a macro or modal READ returns `busy-modal`; this version does
not defer macro invocation. Direct textual INVOKE/RUN uses the active source
at that explicit command boundary.

The fixed effective settings are session working directory, inherited process
environment, captured source bytes and the interpreter's own included-file
search starting from the staged source pathname. A snapshot does not preserve
the original source directory for relative `::requires` or imports. Programs
requiring the original saved source location must wait for a separately
implemented `SOURCE FILE` contract; no implicit Save is performed.

Executable and arguments are data passed directly to process creation. Quotes
inside command operands are doubled; backslashes remain data. Empty quoted
ARGs remain actual arguments. Limits are 32 tools, 128 argv entries, 2 MiB
captured source and 8,191 bytes in the argv joined with single spaces. This
conservative bound protects ooRexx's native CLI argument buffer and also applies
to generic native-process tools. The existing 8,192-byte complete command and
2,048-byte operand limits can be stricter for textual declarations. Capability
inspection reports these bounds. ACTIONS pages expose tool/source/argument
count; they do not expand stored argv into unbounded inspection pages.

ooRexx's CLI retains its traditional Rexx argument string and supplies separate
process arguments through `.SYSCARGS`. Completion is **process-exit-only**:
the OS exit status is retained, nonzero exits and signals fail the job, and
stderr is displayed as program data. A CLI condition is not decoded into a
typed Rexx condition, and its diagnostic text is never parsed to infer one.
POSIX exit-status truncation remains visible; full language-condition data
needs a separate API adapter. Stop terminates the owned process group/job and
cleans the source stage. An interpreter's independent provider services, such
as ooRexx `rxapi`, retain their own runtime lifecycle.

The existing named LOG routes and [native FILE routes](workbench-file-io.md)
apply to these tools. Finite FILE stdin preserves bytes and drains EOF. Failed
stdin delivery, nonzero exit, signal, Stop, output limit or host failure prevents
`CREATE PUBLISH SUCCESS`; no partial output is published.

## Executable checks

`test_workbench_native_tools_runtime` always uses a generic source interpreter
fixture on the qualified host and checks ownership, candidate publication,
explicit invocation, missing dependencies, argv, pending source, exact FILE IO
and failure preservation. `test_workbench_native_tools_curses` selects a TOOL
action from the real native menu on a PTY. `test_inputevent` checks source-pin
FIFO preservation; the test-only `test_native_tool_queue` driver exercises the
real editor's stale-source dispatch after a queued switch and source close. The restricted
native browser and rebuilt Wasm browser harness check typed declaration/action/
RUN refusals without editor mutation.

Actual ooRexx qualification is optional and explicit:

```sh
THE_TEST_OOREXX_EXECUTABLE=/absolute/ooRexx/bin/rexx ctest --test-dir cmake-build-debug --output-on-failure -R 'test_native_process|test_workbench_native_tools'
```

The actual-runtime probe uses that executable and its matching adjacent `rxapi`,
with an isolated short runtime directory and an owned provider process. It
checks object dispatch, `.SYSCARGS`, binary finite stdin and editor preservation.
The component test additionally covers conditions, Stop and the 8,191-byte
boundary. Omitting the variable does not establish ooRexx runtime qualification.
These checks do not qualify installation, signing, packaging or release assets.
