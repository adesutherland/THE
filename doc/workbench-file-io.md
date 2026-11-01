# Native workbench FILE routes

The native POSIX host supports bounded exact-byte FILE stdin and one staged
`CREATE` output sink per program job. These routes use the existing Classic
and RXBIN tool lifecycle. Windows, browser-native restricted sessions and
browser/Wasm report the capability unavailable. `REPLACE` and `APPEND` are
explicitly unsupported until an adapter can exclude concurrent external writers
through conditional atomic publication.

## Define and inspect

Definitions are owner-bound templates inside the usual transaction. Defining,
committing, inspecting or removing a route neither starts a program nor creates
its output target. A later `RX RUN` copies the templates for that job; changing
them cannot retarget a running program.

```text
WORKBENCH BEGIN OWNER public.fileio
WORKBENCH ROUTE PUT public.input FROM FILE "input.bin" TO tool:classic-rexx/stdin FORMAT BYTES EOF END
WORKBENCH ROUTE PUT public.output FROM tool:classic-rexx/stdout TO FILE "result.bin" FORMAT BYTES MODE CREATE PUBLISH SUCCESS
WORKBENCH COMMIT
QUERY WORKBENCH ROUTES
QUERY WORKBENCH CAPABILITIES
QUERY WORKBENCH FILEIO
WORKBENCH FILE INSPECT "result.bin"
QUERY WORKBENCH FILE "result.bin"
RX RUN
```

Output can select `stderr` instead of `stdout`. A tool may have one FILE stdin
producer, one FILE output sink, and its decoded LOG routes. A second FILE output
sink is refused even when it selects the other stream. File paths must be quoted;
quotes inside a path are doubled. Paths are data, never shell redirection or
command text. Commit validates the canonical parent/workspace without creating
files. An absent input leaf is allowed in a template; route inspection reports
its current unavailability and reason, and launch rechecks it.

The initial native workspace is the canonical directory in which THE started.
Relative FILE operands use that fixed directory after editor directory changes.
Absolute operands must remain within it. Inspect `fileWorkspace` in
`QUERY WORKBENCH CAPABILITIES`, or `workspace` in `QUERY WORKBENCH FILEIO`.
Native file authority does not follow from disabled browser frontend policy.
A chooser or token cannot expand it.

FILE input and inspected existing files must be regular files. Final symlinks
are refused, including dangling output symlinks. The host holds the startup
workspace directory and traverses canonical parents through directory descriptors
without following changed symlinks. Parent/source identity is rechecked around
capture and parent identity is rechecked at publication. Input/output aliases,
inaccessible paths and readonly output parents refuse launch. Session readonly
mode also prevents FILE output.

Every output target already open in THE is refused, including a clean new
buffer naming an absent file. This first increment protects pending records
and staged file-field projections without leaving a clean buffer silently stale.
Close that buffer before using the output target. Existing editor Save, reload
and dirty-state behavior is unchanged.

## Exact bytes and completion

At launch the host captures an immutable input snapshot of at most 8 MiB.
Capture failure prevents program launch. The snapshot is queued without repair,
conversion or a newline, followed by one EOF after its bytes drain. Empty input
sends EOF without bytes. Interactive `RX INPUT` and further EOF are disabled for
that job; Stop remains available. Input delivery failure prevents successful file
publication. Later source-file changes do not change the snapshot. A program or
interpreter may apply its own encoding rules after receiving those bytes.

The selected output stream is captured before UTF-8 display decoding. NUL,
invalid UTF-8 and bytes split between messages remain exact in staging. The
Program console and LOG routes remain separate decoded projections. The 8 MiB
aggregate job-output ceiling includes both streams; file staging also has an
8 MiB ceiling. Retained text limits do not trim file capture.

`CREATE PUBLISH SUCCESS` keeps the target absent while the program runs. The
host stages private `0600` bytes in an unpredictable `0700` subdirectory held
by descriptor beneath the destination parent, syncs the complete file, checks
the staged entry's identity and publishes with an atomic no-clobber hard link.
A racing target creation wins unchanged and fails this publication. Moving or
replacing the staging directory cannot retarget its held descriptor. The host
verifies the published target and parent, then acknowledges completion with
the byte length and a new opaque token. Code running with the same operating
system user identity, or greater privileges, can defeat private-file permissions;
malicious tampering by such code is outside this qualification boundary.

Runtime failure, nonzero status, interpreter conditions, Stop, output limit,
changed parent, policy refusal or confirmed storage failure discards staging.
Confirmed pre-publication failure retains target bytes or absence. A successful
interpreter exit whose publication fails produces a failed job, never a
successful output-publication result.

`QUERY WORKBENCH FILEIO` exposes the latest file-job receipt: job identity,
code/message, canonical target, input/output lengths, `published` and the
acknowledged token. File inspection exposes canonical path, existence, readonly
state, byte length and token. Tokens describe a version; they grant no authority
and do not enable deferred replacement modes.

If acknowledgement or final path verification fails after the link was created,
the receipt says `publication-unknown` and makes no unchanged-target claim.
All later FILE output is blocked until explicit `WORKBENCH FILE INSPECT` or
`QUERY WORKBENCH FILE` reconciles that target. Inspect it before deciding whether
another write is needed; blind replay is refused.

## Refusal and verification boundaries

Stable codes include `unsupported-feature`, `invalid-operand`, `policy-denied`,
`object-in-use`, `limit-exceeded`, `stale-revision`, `io-failure` and
`publication-unknown`. Collisions and destination changes use `stale-revision`.
Unsupported modes never fall back to an unguarded rename, live append,
decoded-log copy or shell redirection.

`test_workbenchfileio` executes binary/empty input, fixed-input EOF, raw chunks,
Stop/start/reset cleanup, quota, injected storage/acknowledgement faults,
collisions, symlinks, workspace/readonly refusal, parent swaps, private staging
identity and read-back reconciliation. `test_workbench_fileio_runtime` uses the
real `the --driver llm` editor and native Classic runner for grammar, published
bytes, capabilities/receipts, EOF and editor target protection. Platform execution,
browser refusal, installation and release qualification remain separate checks.
