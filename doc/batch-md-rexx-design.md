# Batch Markdown REXX Example Rendering

## Goal

Provide a batch workflow that turns a Markdown document containing embedded
REXX or THE/CREXX examples into a formatted, validated artifact.

The workflow should:

- read a Markdown source file.
- find fenced example blocks with explicit attributes.
- extract and syntax-highlight the REXX source using THE's existing parser and
  colour/style model.
- run examples where requested.
- capture and format the example output.
- fail the batch run when an example has parser diagnostics, compile errors, or
  unexpected runtime output.
- produce a formatted document, with HTML as the first concrete target.

The implementation should keep document orchestration in REXX scripts where
possible. C should expose stable editor primitives for parser completion,
style-span extraction, diagnostics, and other state that scripts cannot safely
infer.

## Existing Pieces

Current support that should be reused:

- `the -b -q -p profile.the file` already runs THE in batch mode and applies a
  profile/macro to files.
- CREXX profiles/macros use `ADDRESS THE` and the normal THE command
  dispatcher.
- `SDSLHWAIT [milliseconds]` already waits for SDSLH parsing to complete.
- `EXTRACT /PMSGS/` exposes SDSLH diagnostics.
- `EXTRACT /MESSAGES/` exposes THE message history.
- `INPUTSTEM` lets CREXX scripts insert generated lines into a buffer.
- CREXX can use its own OS/process and pipe I/O facilities to run standalone
  examples and capture stdout/stderr.
- Current CREXX driver/runtime behavior is suitable for batch validation:
  `exit n` and integer `main` returns propagate to the process status, compiler
  diagnostics go to stderr, `ADDRESS COMMAND` updates `rc`, `lines()` returns
  `-1` for missing files, and final newlines do not synthesize extra empty
  records.
- `the --driver llm` already exposes visible logical style spans as JSON, which
  is useful for prototyping and tests.

Important constraint: interactive helper macros such as `crexxrun.the` are not
the right batch foundation because THE batch mode rejects operating-system
commands. That does not require THE itself to grow a process runner: the batch
CREXX script can run standalone examples through CREXX's own OS/pipe support,
then feed captured source and output back through THE buffers for highlighting.

## CREXX Validation Notes

These notes capture CREXX behavior that the batch Markdown work depends on or
has stress-tested. They are not current THE blockers, but they should remain
visible while CREXX is changing.

Validated or fixed behavior in the installed CREXX build
(`1ad89445a` or newer):

- Driver failures propagate to the host process: `exit n`, integer `main`
  returns, compile failures, and runtime failures surface as non-zero process
  status.
- Compile diagnostics are written to stderr, and compile errors return
  non-zero, so wrappers can distinguish diagnostics from successful program
  output.
- Compiler warnings are labelled as warnings, not as `rxc` errors.
- `ADDRESS COMMAND` updates `rc` with the shell command status.
- `lines()` and `linein()` no longer synthesize an extra empty record at EOF
  for files ending in a newline.
- Missing-file handling is distinguishable: `lines(missing)` returns `-1` and
  reports to stderr, while `linein(missing)` reports to stderr and exits
  non-zero through the runtime failure path.
- `lineout(path, value)` output is visible to immediate consumers. The batch
  tools now write generated source, profiles, protocol files, and final output
  directly with `lineout`.
- Standalone CREXX accepts stem indexing inside concatenated expressions, such
  as `'emsg ' || stylespans[i]`.
- Ordinary comparisons now follow Rexx loose comparison rules for
  numeric-looking strings; strict comparisons remain exact.
- Multiple exposed array arguments now write back reliably. The renderer now
  passes execution results through separate exposed stdout, stderr, message,
  and metadata stems instead of one tagged capture stem.
- A hosted THE profile rerun confirms that
  `ADDRESS COMMAND "sh" input command_lines output out error err` compiles and
  runs in the hosted profile path.

Remaining hosted THE/CREXX integration observations:

- Shell metacharacters such as redirection are not interpreted by
  `ADDRESS COMMAND` unless the command is explicitly routed through a shell.
  For complex command text, use the documented stdin-fed form:
  `ADDRESS COMMAND "sh" input command_lines`.
- Direct stem indexing inside a concatenated `ADDRESS THE` command expression
  in a hosted profile still fails in the THE profile compile path. A reduced
  profile containing `'emsg THE_BATCH_RUN_MESSAGE=' || messages[i]` still
  reports `#UNEXPECTED_ARRAY` under
  `the -b -q -p stem_concat_profile.the scratch.txt`, while assigning
  `messages[i]` to a scalar first works. The renderer keeps that scalar
  temporary in generated hosted profiles.
- `exit n` and `lineout(...)` are accepted in hosted profiles, but THE batch
  mode still returned process status `0` for a profile that executed `exit 7`
  and also reported `Error 0077: Files still open in batch: 1`. This is a
  THE-hosted profile/batch propagation issue, not a standalone CREXX runner
  blocker.

## Recommended Shape

Use a two-layer design:

1. A CREXX/REXX batch document pipeline owns Markdown scanning, block metadata,
   temporary files/buffers, example execution policy, output comparison, and
   final rendering. It also owns standalone OS/process execution through CREXX
   facilities.
2. THE C internals expose small, deterministic query/command primitives:
   parser wait, parser diagnostics, and full-buffer style spans.

This keeps the renderer flexible while avoiding fragile REXX attempts to read
terminal colours, visible screen rows, or parser internals.

Preferred execution flow:

1. CREXX scans the Markdown and writes each runnable source block to a temporary
   file or scratch buffer.
2. CREXX uses `ADDRESS THE` to `EDIT` the source buffer, run `SDSLHWAIT`, read
   `PMSGS`, and extract style spans.
3. When `run=true`, CREXX runs the standalone example using its own OS/pipe I/O
   support and captures return code, stdout, and stderr.
4. Captured output is loaded into THE only when it needs syntax highlighting or
   buffer-based formatting.
5. CREXX combines the preserved Markdown, highlighted source, captured output,
   and validation results into the final artifact.

## Source Format

Start with ordinary fenced Markdown blocks. The fence info string carries the
minimum metadata needed by the runner. The concrete Story 1 source contract is
documented in [Batch Markdown REXX Example Format](batch-md-rexx-format.md).

````markdown
```rexx id=hello run=true output=text
options levelb
say "hello"
```
````

Proposed attributes:

- `id`: stable example id, required when output is checked.
- `run`: `true` or `false`; default `false`.
- `kind`: `standalone`, `the-macro`, or `address-the`; default `standalone`.
- `output`: `text`, `markdown`, `json`, `html`, or another parser name;
  default `text`.
- `expect`: optional path or inline expectation id for output comparison.
- `timeout`: runtime timeout in milliseconds.
- `allow-rc`: expected non-zero process return code, or `*` to accept any
  return code; default `0`.
- `fail-on-diagnostics`: default `true`.

The first parser should be line-oriented and conservative. It does not need to
be a complete Markdown parser; it only needs to preserve non-example lines and
recognize fenced blocks.

## Output Model

Use a small intermediate model rather than rendering directly while scanning:

- `MarkdownSegment`: unchanged source Markdown.
- `ExampleSource`: source text plus style spans and diagnostics.
- `ExampleRun`: return code, stdout, stderr, THE messages, elapsed time.
- `RenderedOutput`: output text plus optional style spans or rendered Markdown.

HTML should be the first renderer because THE's logical syntax categories map
cleanly to CSS classes. TeX/PDF can be added later from the same intermediate
model once the flow is proven.

Recommended HTML source rendering:

```html
<pre class="the-example the-example-rexx"><code>
<span class="syn-keyword">address</span> the
</code></pre>
```

Recommended output rendering for the MVP:

- `output=text`: escaped `<pre class="the-output">`.
- `output=json`, `output=rexx`, etc.: highlight by feeding the captured output
  through THE/SDSLH as a temporary buffer, then render spans.
- `output=markdown`: render a small Markdown subset or pass through a later
  dedicated Markdown renderer. Do not block the first MVP on full Markdown
  rendering.

## Minimum C Surface

Already available:

- `SDSLHWAIT [milliseconds]`
- `EXTRACT /PMSGS/`
- `EXTRACT /MESSAGES/`

Needed for a robust REXX implementation:

### `EXTRACT /STYLESPANS/`

Expose style runs for the current file independent of what is visible on
screen. This should use the same logical style taxonomy as the LLM driver
(`keyword`, `string`, `comment`, `function`, `operator`, and so on).

Candidate stem format:

```text
stylespans.0 = number of records
stylespans.1 = line start-cell cell-count style
stylespans.2 = line start-cell cell-count style
```

Implemented Story 3 contract:

- The item name is `STYLESPANS`.
- `EXTRACT /STYLESPANS/` returns all available style spans for the current
  file.
- `EXTRACT /STYLESPANS start/` returns spans for one file line.
- `EXTRACT /STYLESPANS start end/` returns spans for the inclusive file-line
  range.
- Stem records use one-based file lines and zero-based cells:
  `line start-cell cell-count style`.
- Style names are lowercase logical categories matching the LLM driver, such
  as `preprocessor`, `identifier`, `keyword`, `string`, and `comment`.
- The first implementation is full-buffer for SDSLH-backed highlighting. When
  SDSLH/full-buffer highlighting is unavailable, `stylespans.0` is `0`.

### Optional Later Primitive: THE-Owned Process Execution

The preferred MVP route is for CREXX to own standalone process execution. If
that proves insufficient, add an explicit batch-safe THE command or CREXX bridge
for process execution with captured stdout/stderr stems. Do not reuse
interactive `OS` semantics without a security and portability review.

This should be treated as a fallback, not part of the first implementation.

## Story Map

### Story 1: Document the Batch Example Format

As a documentation author, I can mark REXX examples in Markdown with stable
metadata so the batch runner knows which blocks to highlight, run, and verify.

Acceptance criteria:

- A sample Markdown file demonstrates `run=false`, `run=true`, and output
  attributes.
- Invalid or duplicate `id` values produce clear diagnostics.
- Ordinary Markdown is preserved byte-for-byte outside generated sections.

### Story 2: Prototype Highlight Extraction

As a maintainer, I can prove that THE can produce stable style spans for a
temporary REXX buffer in batch/headless operation.

Acceptance criteria:

- `SDSLHWAIT` is used before reading diagnostics or style spans.
- Parser diagnostics fail the run when `fail-on-diagnostics=true`.
- A test proves style categories are stable for a small REXX fixture.
- The prototype may use `the --driver llm` JSON, but the product path is a REXX
  query/command API.

Implementation status:

- `tools/batch-md-rexx/highlight.crexx` drives `the --driver llm` against a
  source file, registers an SDSLH parser, enables automatic colouring, runs
  `SDSLHWAIT`, and emits a line-oriented span manifest.
- Parser diagnostics are read from the LLM snapshot. The prototype exits `1`
  when diagnostics are present and `fail-on-diagnostics=true`.
- The test fixture in `tools/batch-md-rexx/tests/test_highlight.sh` validates
  stable REXX categories for preprocessor text, identifiers, keywords, strings,
  and comments.
- The prototype writes the LLM protocol with CREXX `lineout(path, value)` and
  closes the file before invoking THE, validating the fixed file visibility
  behavior.

### Story 3: Add Full-Buffer Style Span Query

As a REXX macro author, I can extract syntax style runs for any line range in
the current buffer without depending on the visible screen.

Acceptance criteria:

- `EXTRACT /STYLESPANS/` or final chosen name returns a stem.
- It works in batch mode.
- It returns zero records when highlighting is unavailable rather than crashing.
- Tests cover plain ASCII, UTF-8 text, and at least two syntax categories.
- Documentation explains line/cell indexing and style names.

Implementation status:

- `EXTRACT /STYLESPANS/` and `EXTRACT /STYLESPANS start [end]/` are implemented
  for SDSLH-backed current files.
- The extractor reads the SDSLH `CodeBuffer` directly and sets the REXX stem
  itself, so it is not limited by the small fixed `item_values` array used by
  simple query items.
- The result is independent of visible rows and returns zero records when
  highlighting is off, unavailable, or not SDSLH-backed.
- `tests/test_stylespans_extract.sh` validates batch extraction with `rxc`,
  including ASCII categories, a UTF-8 string line, range extraction, and the
  zero-record unavailable case.

### Story 4: Build the REXX Markdown Scanner

As the batch renderer, I can split a Markdown file into normal Markdown segments
and example blocks.

Acceptance criteria:

- Fenced blocks are recognized with attributes.
- Non-REXX fences are preserved as normal Markdown.
- Unterminated fences and malformed attributes produce useful errors.
- Unit-style fixtures cover common Markdown edge cases.

Implementation status:

- `tools/batch-md-rexx/render-html.crexx --scan` emits a line-oriented scanner
  manifest with `markdown` segment records and `example` block records.
- The scanner validates the Story 1 attribute contract, duplicate ids,
  unterminated fences, quoted attributes, tilde fences, indented fences, and
  malformed attributes.
- Non-REXX fences remain part of surrounding Markdown records rather than
  becoming examples.
- `tools/batch-md-rexx/tests/test_scan.sh` covers common scanner edge cases in
  CTest.

### Story 5: Render Highlighted Source to HTML

As a reader, I see source examples with THE/SDSLH syntax highlighting in the
generated HTML.

Acceptance criteria:

- HTML output escapes source text correctly.
- Style spans become stable CSS classes.
- Unknown styles degrade to plain text.
- The renderer includes a minimal CSS theme based on existing `ECOLOR` style
  names, not terminal colour escape sequences.

Implementation status:

- `tools/batch-md-rexx/render-html.crexx` renders HTML to stdout in default
  mode.
- For each REXX/CREXX/THE example block, the renderer writes the source to a
  temporary file, runs THE in batch mode with a generated profile, waits with
  `SDSLHWAIT`, extracts `PMSGS` and `STYLESPANS`, and renders semantic
  `<span class="syn-...">` source markup.
- Source text and Markdown passthrough text are HTML-escaped. Unknown style
  names are emitted as escaped plain text.
- The CSS uses semantic ECOLOR/SDSLH names such as `comment`, `string`,
  `keyword`, `identifier`, and `preprocessor`; it does not emit terminal escape
  sequences.
- `tools/batch-md-rexx/tests/test_render_html.sh` validates escaping,
  highlighted source, preserved non-REXX fences, and parser-diagnostic failure.

### Story 6: Execute Examples

As a documentation author, I can opt in to running an example and see captured
output in the generated artifact.

Acceptance criteria:

- `kind=the-macro` examples run through THE's macro/command path.
- Standalone examples run through CREXX's own OS/process and pipe I/O support.
- stdout, stderr, return code, and THE message history are captured.
- Captured stdout/stderr can be loaded into THE buffers when output highlighting
  is requested.
- Timeouts and non-zero return codes fail the batch unless explicitly allowed.

Implementation status:

- `run=true kind=standalone` writes the block to a temporary file and executes
  it with CREXX through `ADDRESS COMMAND`, capturing stdout, stderr, return
  code, and timeout state.
- `run=true kind=the-macro` and `run=true kind=address-the` run through a
  generated THE batch profile and capture THE message history with
  `EXTRACT /MESSAGES/`.
- Captured stdout/stderr render as escaped output blocks. When `output` names a
  supported example language such as `rexx`, stdout is highlighted through the
  same `STYLESPANS` path as source examples.
- `allow-rc=N` explicitly accepts a non-zero return code; otherwise non-zero
  return codes and timeouts fail the batch run.
- `tools/batch-md-rexx/tests/test_execute.sh` validates standalone execution,
  highlighted stdout, THE macro/address-the execution, message capture, and
  `allow-rc`.

### Story 7: Validate Expected Output

As a maintainer, I can use the batch renderer as a regression test for examples.

Acceptance criteria:

- Expected output can be stored inline or in fixture files.
- Mismatches show the example id and a small diff.
- The process exits non-zero on validation failure.
- Generated artifacts are not updated silently during validation mode.

Implementation status:

- `expect=inline:<text>` validates one-line stdout directly from the fence
  metadata.
- `expect=<path>` validates stdout against a fixture path resolved relative to
  the Markdown source file.
- `--validate` runs highlighting, execution, and expected-output checks without
  emitting generated HTML.
- Mismatches report the example id, line number, expected line, actual line,
  and non-zero process status.
- `tools/batch-md-rexx/tests/test_validate.sh` validates inline expectations,
  fixture-file expectations, mismatch diagnostics, and fixture non-mutation.

### Story 8: Highlight or Render Output

As a reader, I can distinguish example output from source, and structured output
can be highlighted or rendered.

Acceptance criteria:

- `output=text` renders safely as escaped preformatted text.
- `output=<parser>` can feed output through THE highlighting.
- `output=markdown` supports either a documented subset or an external renderer
  path.
- Output rendering never executes embedded HTML unless explicitly trusted.

Implementation status:

- `output=text` and unrecognized output names render as escaped preformatted
  text.
- `output=markdown` renders a safe documented subset: headings, unordered
  lists, blockquotes, fenced code, simple pipe tables, paragraphs, and inline
  backtick code. Embedded HTML is escaped, not executed.
- `output=<parser>` feeds stdout through THE highlighting only when the output
  name is one of the known REXX/THE aliases or exactly matches the parser
  configured by the command line. This avoids treating a Markdown attribute as
  an arbitrary parser command.
- `tools/batch-md-rexx/tests/test_output_render.sh` validates Markdown output,
  parser-highlighted output, and safe escaping of HTML-shaped output.

### Story 9: Package the Batch Profile/Runner

As a user, I can run one command to produce the formatted document.

Candidate command:

```sh
the -b -q -p batch_md_rexx.the -a "input.md output.html" input.md
```

Acceptance criteria:

- The runner prints concise progress and errors.
- Missing CREXX or SDSLH support is reported clearly.
- The install target includes the batch profile and helper macros when enabled.
- CI has at least one end-to-end generated HTML fixture.

Implementation status:

- `tools/batch-md-rexx/the-batch-md-rexx` is the packaged command entry point.
  It locates the installed/source tool directory and launches the CREXX runner.
- `tools/batch-md-rexx/batch-md-rexx.crexx` owns runner validation, progress
  messages, renderer invocation, HTML output-file writing, `--scan`, and
  `--validate`.
- CMake copies the runner and helper CREXX scripts into the build release
  directory and installs `the-batch-md-rexx` to `bin` plus the helper scripts to
  `share/the/batch-md-rexx`.
- `tools/batch-md-rexx/tests/test_runner.sh` validates the packaged command in
  render and validation modes.
- The THE profile candidate remains a later item. Hosted profiles now support
  the process I/O stem shape, but the profile compile path still rejects direct
  stem indexing inside concatenated `ADDRESS THE` command expressions, and THE
  batch mode still needs reliable profile failure/status propagation.

### Story 10: Add TeX/PDF Renderer

As a documentation publisher, I can generate a print-oriented artifact from the
same validated examples.

Acceptance criteria:

- The renderer consumes the same intermediate model as HTML.
- Source and output styles map to TeX macros or listings.
- The TeX renderer is optional and does not complicate the HTML MVP.

## Suggested Milestones

### Milestone A: Design Spike

- Finalize fence attributes and output policy.
- Decide the style-span query name and indexing.
- Build one throwaway proof using current LLM JSON or visible style spans.

### Milestone B: C Primitives

- Implement/document full-buffer style span extraction.
- Add deterministic tests around batch highlighting and diagnostics.

### Milestone C: REXX MVP

- Implement scanner, source highlighter, HTML renderer, and basic text output.
- Support `run=false` and `kind=the-macro`.
- Generate a sample HTML artifact in CI.

### Milestone D: Validating Examples

- Add standalone CREXX execution through CREXX OS/pipe facilities.
- Add expected-output comparison and failure reporting.
- Highlight structured output.

### Milestone E: Additional Renderers

- Add Markdown-output rendering improvements.
- Add TeX/PDF when HTML behavior is stable.

## Key Decisions To Make Early

1. Which CREXX OS/pipe API should the runner standardize on for captured
   stdout, stderr, return code, and timeout behavior?
2. Should generated output be inserted into a derived artifact only, or should
   the tool also support updating checked-in Markdown fixtures?
3. What is the final name and exact stem format for style-span extraction?
4. How much Markdown should `output=markdown` support in the MVP?
5. Should HTML CSS use semantic style classes only, or optionally export the
   current `ECOLOR` theme values too?

## Recommended First Implementation Path

Start with HTML and semantic CSS classes. Add full-buffer style span extraction
as the main C change. Build the scanner and renderer in CREXX. Use `ADDRESS THE`
to load source/output buffers, wait for highlighting, extract diagnostics and
style spans, and use CREXX's own OS/pipe support to run standalone examples.

This gives a useful end-to-end tool quickly while preserving the intended
boundary: THE C owns editor/parser truth; CREXX owns document workflow and
process orchestration.
