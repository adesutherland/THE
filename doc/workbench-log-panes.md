# Named workbench log panes

Phase 2A adds C-owned LOG panel definitions, retained text models and program
stdout/stderr routes. It builds on the [action/menu transactions](workbench-actions-menus.md).
[Read-only DOCUMENT panes](workbench-document-panes.md) add live buffer views
through a separate bounded-read service. Forms, file routes, new execution
adapters and concurrent program jobs remain later phases of the
[workbench design](workbench-extension-design.md).

## Declare, then show

These commands declare one log, two stream routes and a menu-callable Show
action. Put them in a profile or explicitly invoked macro using the existing
`ADDRESS THE` bridge; cREXX addressed literals need semicolons.

```text
WORKBENCH BEGIN OWNER user.logs
WORKBENCH PANEL PUT user.output KIND LOG MODEL user.programlog
WORKBENCH PANEL PLACE user.output RIGHT FALLBACK BELOW,TAB MINCOLS 32
WORKBENCH ROUTE PUT user.stdout FROM tool:classic-rexx/stdout TO log:user.programlog
WORKBENCH ROUTE PUT user.stderr FROM tool:classic-rexx/stderr TO log:user.programlog
WORKBENCH ACTION PUT user.show LABEL "Show output" PANEL user.output OP SHOW
WORKBENCH COMMIT
```

Definitions do not launch a program, show a pane or take focus. Outside profile
activation, successful commit creates any missing empty models and publishes
one definition revision. A profile's inner commits remain candidates until the
whole activation succeeds; a later failure creates no candidate models and
retains the previous published graph.

After activation, explicitly request presentation and run through existing RX:

```text
WORKBENCH PANEL SHOW user.output
RX RUN classic-rexx
```

`SHOW` changes visibility without focusing. `FOCUS` requires the panel to be
visible; `HIDE` removes its presentation without clearing text or stopping its
producer. Runtime operations against an unpublished profile candidate return
`activation-pending`. Modal READ retains input ownership and returns `busy-modal`.
In the browser, showing a log from Program input displays Program and that log
in the auxiliary rail so the input remains visible and keeps its draft and
typing focus. Explicit FOCUS transfers focus to the log. Hiding Program lets
the log fill the rail.

## Implemented grammar

```text
WORKBENCH PANEL PUT panel-id KIND LOG MODEL model-id
WORKBENCH PANEL PLACE panel-id RIGHT|BELOW|TAB FALLBACK BELOW,TAB MINCOLS integer
WORKBENCH PANEL REMOVE panel-id
WORKBENCH PANEL SHOW panel-id
WORKBENCH PANEL HIDE panel-id
WORKBENCH PANEL FOCUS panel-id
WORKBENCH ACTION PUT action-id LABEL "label" PANEL panel-id OP SHOW|HIDE|FOCUS

WORKBENCH ROUTE PUT route-id FROM tool:tool-id/stdout TO log:model-id
WORKBENCH ROUTE PUT route-id FROM tool:tool-id/stderr TO log:model-id
WORKBENCH ROUTE REMOVE route-id

QUERY WORKBENCH PANELS|ROUTES|LOGS [OFFSET integer] [LIMIT integer]
QUERY WORKBENCH LOG model-id [EXPECT generation] [CURSOR cursor] [OFFSET byte-offset] [LIMIT integer] [BYTES integer]
```

`EXTRACT /WORKBENCH .../` accepts the same inspection selectors. Definitions and
placement require `BEGIN`/`COMMIT`; SHOW/HIDE/FOCUS are runtime requests against
published panels. The internal generation-checked RETURN event restores the
invoking control while leaving a focused log visible; RETURN has no public
command or action grammar.

Placement defaults to RIGHT, fallbacks BELOW then TAB, minimum width 32 columns
and minimum height four rows. `PLACE` supplies a preferred placement, a comma
list of distinct alternatives, and a width from 1 through 32,767. A fallback
cannot repeat the preferred placement. Physical placement depends on the active
renderer: inspection separates the request from `effectivePlacement` and its
reason. Without a renderer, the semantic request remains inspectable with
`unavailable-renderer`. The current command grammar uses the panel ID as its
title. Tool/model IDs are case-sensitive; use the shown `tool:`, `log:`,
`stdout` and `stderr` resource spellings.

An inactive pane in a tabbed auxiliary container reports `TAB` with
`inactive-tab` as its presentation state, even if `TAB` was not requested as
an active-pane fallback. The selected pane still follows its declared placement
and fallback order; a placement that cannot fit returns its actual fallback or
`NONE` with a reason.

## Ownership and lifetime

A panel creates its named model if absent, or attaches to the same owner's
existing log. Several panes can display one model without duplicating appended
records. Cross-owner model attachment and dangling panel-action/model references
are refused at commit. Each tool stream has at most one primary log route.
Missing tools refuse; an installed catalog entry that is currently unavailable
can retain a template, with launch availability checked again later. Only
program, runner and compiler roles supply these routes. Profile output, stdin
echo and DSLSH frames are separate channels.

A new job snapshots its route bindings before the backend starts. Replacing or
removing a route affects later jobs. Hiding/removing panes, or resetting their
owner, retains a running job's bindings and its models. Removing a panel while a
PANEL action still references it fails unless the same transaction removes or
rebinds that action. Removing the focused pane requests a return to the saved
editor/Program origin. Replacing or moving a focused pane also returns to that
origin when its generation changes, retaining visibility and text. Appending
output never changes selection or focus.

Logs retain their owner binding and text after their last pane is removed and
remain discoverable through LOGS. This checkpoint has no public log-clear/delete
command; RX CLEAR still clears its existing Program console rather than named
logs. Retained models count toward the 32-model session limit and can be reused
by their owner. Session reset invalidates live bindings, while observer end still
owns their eventual release; late output cannot enter a recreated generation.

## Inspect bounded text

PANELS and ROUTES use the existing schema-1 counted pages. PANELS includes title,
kind/model, model generation, placement/fallbacks/minima, visibility/focus,
effective placement, content revision and model error. LOGS lists retained model
ownership, generation, content revision, retained/omitted counts, lease count and
any last route error. Panel generations and 64-bit log tokens are decimal JSON
strings; preserve them without JavaScript Number conversion.

LOG content inspection defaults to the oldest retained text, offset zero,
eight records and a 4,096-byte text budget. LIMIT accepts 1–8 and BYTES accepts
1–8,192. EXPECT checks model generation; without it, the current published
model's generation is used. Continue using the returned `nextCursor` and
`nextOffset` until `more` is false. Offsets refer to the original text event and
must lie on UTF-8 scalar boundaries. Evicted/cleared history resumes with
`omitted: true`; generation mismatch refuses with `stale-generation`.
QUERY and EXTRACT preserve its numeric RC -3; a text budget too small for the
next UTF-8 scalar returns `limit-exceeded` RC 94. A failed EXTRACT leaves its
previous WORKBENCH stem values intact.

Each record preserves job ID, stream (1 stdout or 2 stderr), event sequence and
length. Input is decoded display text, including existing UTF-8 repair, rather
than an exact-byte file sink. NUL and terminal/markup controls remain data;
length-aware JSON escapes controls, and renderers must escape presentation rather
than interpret the text as terminal or HTML instructions. Page budgets keep JSON
within its 65,536-byte buffer; failed inspection never publishes truncated JSON.

In the LLM protocol, prefix commands with `command`, for example:

```text
command query workbench panels
command query workbench logs
command query workbench log user.programlog limit 8 bytes 4096
```

LLM capabilities advertise log panes, routes and command-based log inspection.

Definitions, pane state/focus requests and log content have separate revisions.
A route append failure updates inspectable error state and its notification
revision without pretending that content changed or implicitly stopping the job.
Normal retention eviction is represented by omission counters, not a route error.

## Core integration and bounds

[workbench.h](../src/workbench.h) owns transactional definitions and pure tool
metadata lookup. [workbenchruntime.h](../src/workbenchruntime.h) provides pane
state/focus requests, renderer placement feedback and immutable job route leases.
[workbenchlog.h](../src/workbenchlog.h) owns retained UTF-8 records and paging.
Drivers enqueue SHOW/HIDE/FOCUS/RETURN through the normalized panel constructor;
the editor admits them at its input boundary. Borrowed state/text pointers expire
on their owning model's next mutation.

Initial limits are 32 panels, 32 route templates and 32 retained log models;
256 KiB and 128 records per log; 8 MiB aggregate retained model text; and one
active program job. The existing tool host still enforces its own job-output and
input limits. Core/source checks, physical frontend qualification, installed
packages, hosted CI and release publication remain separate boundaries.
