# Batch document renderer architecture

The batch tools turn attributed Markdown example fences into highlighted and,
when requested, executed/validated HTML or TeX. PDF is a subsequent optional
TeX compilation. [The format and runner guide](batch-md-rexx-format.md) defines
the source attributes and public commands; [source highlighting](syntax-highlighting.md)
describes the smaller fragment-only interface for sites and books.

## Ownership

The cREXX sources in `tools/batch-md-rexx/` own Markdown scanning, metadata
validation, example execution, output comparison, template expansion and final
rendering. The command-line wrappers discover dependencies and invoke those
sources or their compiled bytecode. They do not add a second parser or editor.

THE supplies editor primitives: `SDSLHWAIT`, `EXTRACT /PMSGS/`,
`EXTRACT /MESSAGES/`, `EXTRACT /STYLESPANS/`, `EXTRACT /STYLESPANSTEXT/` and
`INPUTSTEM`. Parser reads are synchronized through DSLSH; consumers must not
read physical terminal colors or treat visible rows as full-buffer results.

`STYLESPANS` exposes ordered `line start-cell cell-count style` records through
an array. `STYLESPANSTEXT` exposes the same records as one newline-delimited
scalar, avoiding one variable-pool update per span. Both accept inclusive
start/end line ranges. The renderer walks source and ordered spans in one
advancing pass. Semantic categories become HTML classes or TeX macros through
the selected templates.

## Processing

1. Scan the Markdown and validate fence attributes and unique example ids.
2. Load source into a temporary THE batch session and select its parser.
3. Wait for parsing, collect diagnostics and extract full-buffer style spans.
4. For `run=true`, execute the example with cREXX process facilities, or a
   generated THE profile for hosted examples. Capture status, stdout, stderr
   and editor messages separately.
5. Compare requested expected-output fixtures without rewriting them on failure.
6. Highlight/render captured output according to its `output` attribute.
7. Combine the preserved source document, classified examples, results and
   templates into the output artifact. Emit progress/diagnostics on stderr.

THE batch mode rejects its interactive OS commands. Standalone process
execution therefore belongs to the cREXX pipeline, not `crexxrun.the` or a
native THE process runner. Shell metacharacters require explicit shell
invocation; `ADDRESS COMMAND` alone does not interpret redirection.

Source/profile inputs and outputs are files with bounded lifetime. Stable
profile paths permit CREXXSAA cache reuse; generating a new profile pathname
for every source-fragment request defeats that cache. Packaged compiled
renderer bytecode avoids unnecessary source compilation on warm calls.

## Templates and integration

HTML and TeX template sets live under `tools/batch-md-rexx/templates/`.
Substitutions are already escaped for their output format. Styles can be
included once by the surrounding document; fragment requests normally omit
inline style definitions. Ordinary non-example Markdown and fences retain the
format guide's preservation contract.

The fragment command does not execute source examples. Hosting it behind an
HTTP service, integrating it into another repository's manual build, or adding
parallel workers is an external integration choice. Follow the dependency,
serialization, timeout and fallback contracts in the
[source-highlighting guide](syntax-highlighting.md).

## Verification and limits

`test_batch_md_rexx_*` checks format validation, preservation, scanning,
HTML/TeX rendering, templates, execution/status, expected output, output
highlighting and source fragments. Parser/TeX dependencies are optional only
where the relevant test explicitly reports a skip. Inspect stderr and actual
artifacts as well as process status. See [testing](testing.md),
[known issues](known-issues.md) and the [roadmap](roadmap.md).
