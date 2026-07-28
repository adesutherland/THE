# Batch Markdown REXX Example Format

This is the initial source format for batch-rendered REXX examples. It is
deliberately small: ordinary Markdown remains ordinary Markdown, and examples
are marked with fenced code blocks whose info string contains stable metadata.

## Example Fence

````markdown
```rexx id=hello run=true kind=standalone output=text timeout=5000
options levelb
say "hello"
```
````

The first word in the info string is the language. The batch renderer treats
`rexx`, `crexx`, and `the` fences as examples. Other fenced blocks are preserved
as normal Markdown.

## Attributes

Attributes use `key=value` tokens after the language. Values may be unquoted, or
quoted when they contain whitespace. Supported attributes:

- `id`: required stable example identifier. It must start with a letter and may
  contain letters, digits, `_`, `-`, and `.`.
- `run`: `true` or `false`; default `false`.
- `kind`: `standalone`, `the-macro`, or `address-the`; default `standalone`.
- `output`: output renderer/parser name; default `text`. The first named
  outputs are `text`, `markdown`, `json`, `html`, and `rexx`, but future parser
  names are allowed if they use identifier characters.
- `expect`: optional expected-output fixture name or path.
- `timeout`: optional positive integer timeout in milliseconds.
- `allow-rc`: optional accepted return code, or `*` to accept any return code;
  default `0`.
- `fail-on-diagnostics`: `true` or `false`; default `true`.

Unknown attributes are errors. Duplicate attributes are errors.

Example ids must be unique across the Markdown source. Duplicate ids are errors
and should report both the duplicate fence line and the first definition line.

## Preservation

The parser is line-oriented and conservative. It recognizes fenced examples,
validates their metadata, and keeps every source line as raw input. Renderers
must preserve ordinary Markdown byte-for-byte outside sections they explicitly
generate.

The Story 1 validator can be used to check format metadata and preservation:

```sh
crexx -nokeep tools/batch-md-rexx/validate.crexx -args tools/batch-md-rexx/tests/fixtures/valid.md
crexx -nokeep tools/batch-md-rexx/validate.crexx -args --round-trip tools/batch-md-rexx/tests/fixtures/valid.md > copy.md
```

Validator stdout is data: either manifest records, or exact source text when
`--round-trip` is used. Diagnostics are written to stderr. Exit status `0`
means the file is valid, `1` means the Markdown was read but example metadata
failed validation, and `2` means the command line or input file could not be
processed.

`--round-trip` parses the file and writes the parsed source back out exactly.
The output should compare byte-for-byte with the input, including real trailing
blank lines. A normal final newline is not treated as an additional empty
record.

## Highlight Prototype

Story 2 adds a prototype highlighter for a single REXX source file:

```sh
crexx -nokeep tools/batch-md-rexx/highlight.crexx -args \
  --the cmake-build-debug/the \
  --home cmake-build-debug/release \
  --parser rxc \
  --parser-command "$(command -v rxc)" \
  --parser-arg --syntaxhighlight \
  tools/batch-md-rexx/tests/fixtures/highlight-valid.rexx
```

The prototype emits a manifest rather than final HTML:

```text
highlight source=... parser=rxc sdslhwait=5000 diagnostics=0
span line=1 start=0 len=7 style=preprocessor
span line=2 start=0 len=3 style=keyword
```

Diagnostics are written to stderr. Exit status `0` means spans were produced,
`1` means the parser reported diagnostics or no style spans were available, and
`2` means the highlighter could not invoke CREXX, THE, or the LLM-driver
protocol path. Use `--fail-on-diagnostics false` to inspect spans for a source
file that still has parser diagnostics.

The product path for Story 3 is the editor extract item:

```rexx
stylespans = .string[]
address the "extract /stylespans/" expose stylespans[]
```

`stylespans.0` is the record count. Each subsequent record is
`line start-cell cell-count style`, with one-based file lines, zero-based
cells, and lowercase logical style names such as `keyword`, `string`, and
`comment`. `extract /stylespans start end/` limits extraction to an inclusive
file-line range.

Batch profiles that need the complete result should use the equivalent bulk
scalar extract:

```rexx
stylespanstext = .string
address the "extract /stylespanstext/" expose stylespanstext
```

It returns the ordered records separated by newlines with one variable-pool
transfer. The array form remains useful for interactive profiles and
compatibility; the packaged source highlighter uses the scalar form for large
files.

The focused `the-highlight-source` wrapper turns those same spans into an
embeddable HTML or TeX fragment:

```sh
the-highlight-source --format html-fragment example.crexx > example.html
the-highlight-source --format tex-fragment example.crexx > example.tex
```

TeX is the backward-compatible default. `--include-style true` emits the
corresponding scoped HTML CSS or TeX macro definitions before the fragment;
normal site and book builds should include their tuned definitions once and
leave this option false. Capabilities, dependencies, command syntax, templates,
HTML and TeX integration, and the reverse-proxy contract are documented in the
[Syntax Highlighting User Guide](syntax-highlighting.md).

## Scanner and HTML Renderer

Story 4 adds scanner mode:

```sh
crexx -nokeep tools/batch-md-rexx/render-html.crexx -args --scan examples.md
```

Scanner output is a line-oriented manifest. Markdown records use
`markdown start=N end=N`; example records include the id, language, fence
lines, body range, and validated attributes.

## HTML Templates

The HTML renderer uses default template fragments from
`tools/batch-md-rexx/templates/html/default`. The packaged runner passes that
directory automatically. Custom HTML wrappers can be supplied with:

```sh
the-batch-md-rexx --template-dir path/to/templates/html/default input.md output.html
```

Templates use simple `{{name}}` placeholders. Values inserted by the renderer
are already HTML-escaped; template files should not try to escape them again.
The default template set includes document, Markdown segment, example, source,
run, output, style-token, and `style.css` fragments. Single-source HTML
fragments use the deliberately scoped `fragment-style.css` so their optional
inline style block does not introduce global `body` or document rules.

## TeX and PDF Output

Story 10 adds TeX output from the same validated examples:

```sh
the-batch-md-rexx --format tex examples.md examples.tex
```

The TeX renderer uses default template fragments from
`tools/batch-md-rexx/templates/tex/default`. Custom TeX wrappers can be supplied
with:

```sh
the-batch-md-rexx --format tex --template-dir path/to/templates/tex/default examples.md examples.tex
```

Template values are already TeX-escaped by the renderer. The default TeX
template set maps syntax categories to macros such as `\TheSynKeyword{...}` and
`\TheSynString{...}` in `style.tex`, so TeX styling can be adjusted without
editing the CREXX renderer.

The default TeX Markdown renderer recognizes the same safe subset used for
structured output: headings, paragraphs, unordered lists, blockquotes, fenced
code, simple pipe tables, and inline backtick code. Markdown `#`, `##`, and
`###` headings are emitted through `\TheMarkdownHOne{...}`,
`\TheMarkdownHTwo{...}`, and `\TheMarkdownHThree{...}`. The default TeX document
template does not call `\maketitle`; a Markdown `#` heading is therefore the
visible document title by default. Custom templates can reintroduce a TeX
`\maketitle` flow if desired.

PDF generation is an optional second step:

```sh
the-batch-md-rexx --pdf examples.md examples.pdf
the-batch-md-rexx --pdf --pdf-engine tectonic examples.md examples.pdf
the-batch-md-rexx --pdf --pdf-engine latexmk examples.md examples.pdf
```

`--pdf-engine auto` is the default. It tries `tectonic` first, then `latexmk`.
Only one engine is invoked. If the selected engine is present but fails, the
runner reports that compiler failure instead of falling through to another
engine.

Recommended installations:

- macOS with Homebrew: `brew install tectonic`
- Linux: prefer the distribution `tectonic` package when available; otherwise
  use `latexmk` with a TeX Live installation.
- Existing MacTeX/TeX Live users can select `--pdf-engine latexmk`.

## RXAS Output Highlighting

`output=rxas` renders stdout through the CREXX RXAS SDSLH parser. The packaged
runner looks for `THE_CREXX_RXAS`, then for `rxas` beside the selected `rxc`, and
then for `rxas` on `PATH`. Direct renderer invocations can provide the parser
explicitly:

```sh
crexx -nokeep tools/batch-md-rexx/render-html.crexx -args \
  --the cmake-build-debug/release/the \
  --home cmake-build-debug/release \
  --crexx "$(command -v crexx)" \
  --parser rxc \
  --parser-command "$(command -v rxc)" \
  --rxas-parser-command "$(command -v rxas)" \
  --parser-arg --syntaxhighlight \
  --template-dir tools/batch-md-rexx/templates/html/default \
  examples.md > examples.html
```

The checked-in RXAS toolchain demo requires `rxc`, `rxas`, and `rxdas` on
`PATH`. It compiles a tiny REXX program to RXAS, assembles it to RXBIN,
disassembles it back to RXAS text with `rxdas`, and highlights that output:

```sh
tools/batch-md-rexx/the-batch-md-rexx \
  tools/batch-md-rexx/examples/rxas-toolchain.md \
  /tmp/the-rxas-toolchain-demo.html

tools/batch-md-rexx/the-batch-md-rexx --pdf --pdf-engine tectonic \
  tools/batch-md-rexx/examples/rxas-toolchain.md \
  /tmp/the-rxas-toolchain-demo.pdf
```

Story 5 adds HTML rendering:

```sh
crexx -nokeep tools/batch-md-rexx/render-html.crexx -args \
  --the cmake-build-debug/release/the \
  --home cmake-build-debug/release \
  --parser rxc \
  --parser-command "$(command -v rxc)" \
  --parser-arg --syntaxhighlight \
  examples.md > examples.html
```

The renderer writes HTML to stdout. It preserves non-REXX fences as escaped
Markdown text, highlights REXX/CREXX/THE example source through THE
`STYLESPANS`, and maps known styles to stable `syn-*` CSS classes. Parser
diagnostics fail the run when `fail-on-diagnostics=true`.

Story 6 executes examples when `run=true`. Standalone examples use CREXX via
`--crexx`; `kind=the-macro` and `kind=address-the` run through a generated THE
batch profile. Captured stdout and stderr are rendered below the highlighted
source, and THE message history is included for THE-backed examples. Non-zero
return codes fail the batch unless `allow-rc=N` or `allow-rc=*` is set.

Story 7 adds validation mode:

```sh
crexx -nokeep tools/batch-md-rexx/render-html.crexx -args \
  --validate \
  --the cmake-build-debug/release/the \
  --home cmake-build-debug/release \
  --crexx "$(command -v crexx)" \
  --parser rxc \
  --parser-command "$(command -v rxc)" \
  --parser-arg --syntaxhighlight \
  examples.md
```

`expect=inline:<text>` validates one stdout line. `expect=<path>` validates
stdout against a fixture file resolved relative to the Markdown source. On
mismatch the renderer reports the example id, line number, expected text, and
actual text, then exits non-zero without updating fixtures.

Story 8 renders output according to the `output` attribute:

- `output=text` and unknown output names render as escaped preformatted text.
- `output=rexx`, `output=crexx`, and `output=the` highlight stdout through the
  configured REXX parser.
- `output=rxas` highlights stdout through the CREXX RXAS parser when `rxas` is
  available.
- `output=<parser>` highlights stdout when `<parser>` exactly matches the
  command-line `--parser` value.
- `output=markdown` renders a safe subset: headings, unordered lists,
  blockquotes, fenced code, simple pipe tables, paragraphs, and inline backtick
  code. Raw HTML in Markdown output is always escaped.

Story 9 adds the packaged runner:

```sh
tools/batch-md-rexx/the-batch-md-rexx examples.md examples.html
```

Installed builds provide the same command as `the-batch-md-rexx`. The wrapper
locates the CREXX helper scripts, prints progress to stderr, writes HTML to the
requested output file, and exits non-zero when highlighting, execution, or
validation fails. It also supports validation without generating HTML:

```sh
the-batch-md-rexx --validate examples.md
```

## Sample Blocks

Source-only example:

````markdown
```rexx id=source-only run=false output=text
options levelb
answer = 41 + 1
```
````

Runnable standalone example with plain text output:

````markdown
```rexx id=hello-run run=true kind=standalone output=text timeout=5000 expect=hello-output
options levelb
say "hello from CREXX"
```
````

Runnable example whose output should be treated as Markdown:

````markdown
```rexx id=markdown-output run=true kind=standalone output=markdown
options levelb
say "| name | value |"
say "| --- | --- |"
say "| answer | 42 |"
```
````

THE macro example:

````markdown
```the id=buffer-message run=true kind=the-macro output=text
options levelb
address the
'emsg BATCH_DOC_EXAMPLE'
```
````
