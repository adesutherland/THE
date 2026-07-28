# THE/DSLSH Source Highlighting User Guide

This is the canonical end-user guide to `the-highlight-source`. It describes
the supported highlighting capabilities, dependencies, command syntax, HTML
and TeX output contracts, templates, deployment, and operational use.

The command is intended for documentation authors and services that need
parser-backed highlighting for one cRexx-family source file at a time. The
small runnable example is under
`tools/batch-md-rexx/examples/dslsh-highlight`.

## What the command does

`the-highlight-source` reads a source file and writes one escaped, highlighted
fragment to standard output. The fragment can be embedded in an HTML page or a
TeX document.

```text
source file
    -> cRexx or RXAS parser in syntax-highlighting mode
    -> DSLSH token and diagnostic protocol
    -> THE batch buffer and bulk EXTRACT /STYLESPANSTEXT/
    -> logical styles
    -> HTML classes or TeX macros
```

The parser decides which source ranges are comments, strings, keywords, types,
functions, identifiers, and other logical categories. CSS and TeX macros only
decide how those categories look. This is therefore parser-backed highlighting,
not a regular-expression or CSS approximation of the language.

The command does not:

- serve HTTP itself;
- read source from standard input;
- render a complete HTML page or complete TeX document;
- scan a whole Markdown document; or
- execute the highlighted source.

For complete Markdown documents containing examples, use
`the-batch-md-rexx`. The focused command in this guide is the smaller interface
for an existing website, book generator, or backend service.

## Supported inputs and outputs

| Language | Inferred extension | Parser command |
| --- | --- | --- |
| cRexx | `.crexx` | `rxc --syntaxhighlight` |
| Rexx | `.rexx` | `rxc --syntaxhighlight` |
| THE macro/profile | `.the` | `rxc --syntaxhighlight` |
| RXAS assembly | `.rxas` | `rxas --syntaxhighlight` |

The supported output formats are:

- `html-fragment`: escaped source with stable `syn-*` classes;
- `tex-fragment`: escaped source with stable `\TheSyn...` macros.

Language inference uses the final filename extension and is
case-insensitive. Use `--language` when the input has a different extension.
`.rxpp`, arbitrary Markdown fences, and other languages are not accepted by
this focused command.

Parser diagnostics are treated as a failed highlight operation. Successful
output is written only when the configured parser and THE complete normally.

## Runtime dependencies

The highlighter is an orchestration command. A working deployment needs all of
the following components, built from mutually compatible versions.

| Component | Why it is needed |
| --- | --- |
| `the-highlight-source` | Public command and dependency discovery wrapper. |
| `crexx` | Runs the Level B fragment renderer. |
| `render-html.rxbin` or `render-html.crexx` | Renders HTML or TeX. Packaged Release builds prefer the precompiled image and retain the source as a fallback. |
| `highlight-source-profile.the` | Stable hosted THE profile. Its fixed path lets CREXXSAA reuse its compiled-profile cache across requests. |
| `the` | Owns the source buffer, DSLSH client, parser wait, diagnostics, and style-span extraction. |
| THE runtime resources and drivers | Provide profiles, syntax resources, templates, examples, and the loadable driver used by the batch session. |
| `rxc` with parser mode | Highlights `.crexx`, `.rexx`, and `.the` source. |
| `rxas` with parser mode | Highlights `.rxas` source. The current THE cRexx-enabled build requires it at configuration time; a finished runtime deployment only needs it when RXAS will be requested. |
| DSLSH | Supplies the editor/parser synchronization and token transport used by THE and the parser commands. |
| CREXXSAA ABI 3 or newer | Lets THE execute the generated Level B batch profile. |
| `library.rxbin` and the cRexx import directory | Supply the Level B runtime used by the renderer and hosted profile. |
| POSIX-compatible shell tools | The current wrapper/renderer use `sh`, `env`, `mktemp`, `cat`, and `rm`. |

On native Windows, use a compatible environment such as Git Bash/MSYS for the
current shell wrapper. The THE executable itself supports native Windows, but
`the-highlight-source` is presently a POSIX shell command.

An HTML renderer or web server is not required to produce HTML. A TeX engine is
not required to produce a TeX fragment. To compile the fragment into a PDF,
the consuming document needs a normal TeX installation and the `xcolor`
package used by the default style definitions.

### Build requirements

Build cRexx first, with parser mode enabled so `rxc --syntaxhighlight` and
`rxas --syntaxhighlight` are available. In cRexx this is controlled by
`ENABLE_PARSER_MODE`, which is enabled by default.

Build THE with both integrations enabled:

```sh
cmake -S . -B cmake-build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_SDSLH=ON \
  -DUSE_CREXX=ON
cmake --build cmake-build-release --parallel 10
```

During configuration, THE must report that it found:

- a compatible `crexxsaa` library and headers;
- CREXXSAA ABI 3 or newer;
- `rxc`;
- `rxas`; and
- `library.rxbin` and its import directory.

THE uses a sibling `../DSL-Syntax-Highlighter` checkout when present. Otherwise
it fetches the configured `DSLSH_GIT_REPOSITORY` and `DSLSH_GIT_TAG`. Pin the
tag or commit when a reproducible documentation image is required.

### Installed layout

A normal CMake installation places the relevant files under one prefix:

```text
<prefix>/
  bin/
    the
    the-highlight-source
  lib/the/drivers/
    the_driver_curses...
    the_driver_llm...
  share/the/
    profile.the
    syntax/
    batch-md-rexx/
      render-html.crexx
      render-html.rxbin
      highlight-source-profile.the
      templates/
        html/default/
        tex/default/
      examples/
  share/doc/the/
    syntax-highlighting.md
```

The cRexx installation must likewise make `crexx`, `rxc`, and, when needed,
`rxas` discoverable. THE also needs to resolve the compatible `crexxsaa`
library, `library.rxbin`, and import directory selected at build time.

The build-tree release layout is similar:

```text
cmake-build-release/release/
  the
  the-highlight-source
  drivers/
  doc/
    syntax-highlighting.md
  profile.the
  syntax/
  batch-md-rexx/
    render-html.crexx
    render-html.rxbin
    highlight-source-profile.the
    templates/
    examples/
```

## Command syntax

```text
the-highlight-source
  [--format html-fragment|tex-fragment]
  [--language rexx|crexx|the|rxas]
  [--include-style true|false]
  [--include-wrapper true|false]
  [--template-dir DIRECTORY]
  [--timeout MILLISECONDS]
  SOURCE_FILE
```

The source filename is required and must name a readable file. The command
writes fragment data to standard output and diagnostics to standard error.

### Options

| Option | Default | Meaning |
| --- | --- | --- |
| `--format` | `tex-fragment` | Select HTML or TeX fragment output. TeX remains the compatibility default. |
| `--language` | Inferred from the filename | Override the source language. Required for an unknown extension. |
| `--include-style` | `false` | Emit the default/custom CSS or TeX definitions before the fragment. |
| `--include-wrapper` | `true` | Emit the configured `<pre><code>` or `TheCodeBlock` wrapper. |
| `--template-dir` | Format-specific packaged directory | Use a complete custom fragment template directory. |
| `--timeout` | `5000` | Maximum DSLSH parser wait in milliseconds. This is the parser wait, not an external process-kill timeout. |

Boolean option values are the literal words `true` or `false`.

Advanced parser and THE paths are normally supplied through environment
variables. The underlying renderer also accepts `--the`, `--home`,
`--parser-command`, `--rxas-parser-command`, and `--parser-arg`, but deployment
scripts should prefer the wrapper environment documented below.

### Exit status

| Status | Meaning |
| --- | --- |
| `0` | Highlighting and rendering succeeded. Standard output contains the fragment. |
| `1` | The source parser reported diagnostics. Details are on standard error. |
| `2` | Usage, dependency, template, file, parser-launch, or THE infrastructure failure. |

THE or the cRexx driver may surface a more specific non-zero status for a
lower-level runtime failure. A caller should treat every non-zero status as a
failed response and retain standard error for diagnosis.

## Quick start from sibling Release builds

From the THE repository root:

```sh
export CREXX="$PWD/../CREXX/cmake-build-release/bin/crexx"
export THE_CREXX_RXC="$PWD/../CREXX/cmake-build-release/bin/rxc"
export THE_CREXX_RXAS="$PWD/../CREXX/cmake-build-release/bin/rxas"
export THE_BIN="$PWD/cmake-build-release/release/the"
export THE_HOME_DIR="$PWD/cmake-build-release/release"
```

Generate a self-styled HTML demonstration:

```sh
tools/batch-md-rexx/the-highlight-source \
  --format html-fragment \
  --include-style true \
  tools/batch-md-rexx/examples/dslsh-highlight/hello.crexx \
  > /tmp/hello.thehl.html
```

Generate a TeX fragment for a book build:

```sh
tools/batch-md-rexx/the-highlight-source \
  --format tex-fragment \
  --include-style false \
  tools/batch-md-rexx/examples/dslsh-highlight/hello.crexx \
  > /tmp/hello.crexx.thehl.tex
```

With an installed toolchain on `PATH`, the same commands reduce to:

```sh
the-highlight-source --format html-fragment example.crexx > example.html
the-highlight-source --format tex-fragment example.crexx > example.tex
```

## HTML output and use

The default wrapped fragment has this structure:

```html
<pre class="the-example-source"><code>
<span class="syn-preprocessor">options</span> levelb
...
</code></pre>
```

Source text is HTML-escaped before it enters a template. Template authors must
not escape `{{text}}` a second time.

For a standalone preview, `--include-style true` prepends:

```html
<style>
/* contents of fragment-style.css */
</style>
```

For a website, include the CSS once in the site bundle and render each request
with `--include-style false`. The default CSS is scoped below
`.the-example-source`; it does not set global `body` or page styles.

Use `--include-wrapper false` only when the caller provides its own code-block
container. The returned source lines still contain escaped text and `span`
elements.

## TeX output and use

The default wrapped TeX fragment has this structure:

```tex
\begin{TheCodeBlock}
\TheSynPreprocessor{options} levelb
...
\end{TheCodeBlock}
```

Source text is TeX-escaped before it enters a template. Characters including
backslash, braces, dollar, ampersand, hash, underscore, percent, tilde, and
caret are protected by the renderer. Template authors must not escape
`{{text}}` a second time.

The default `style.tex` uses `\definecolor` and `\textcolor`, so the consuming
document must load `xcolor`. A minimal document integration is:

```tex
\documentclass{article}
\usepackage{xcolor}
\input{path/to/templates/tex/default/style.tex}

\begin{document}
\input{build/the-highlight/example.crexx.thehl.tex}
\end{document}
```

Load `style.tex` once per book and generate individual fragments with
`--include-style false`. `--include-style true` is useful for a small smoke
test, but repeating the definitions for every listing is unnecessary.

`--include-wrapper false` omits `\begin{TheCodeBlock}` and
`\end{TheCodeBlock}` when an existing book macro owns the block environment.

## Logical style contract

The renderer maps DSLSH/THE logical styles to stable HTML classes and TeX
macros:

| Logical style | HTML class | TeX macro |
| --- | --- | --- |
| `comment` | `syn-comment` | `\TheSynComment` |
| `string` | `syn-string` | `\TheSynString` |
| `number` | `syn-number` | `\TheSynNumber` |
| `keyword` | `syn-keyword` | `\TheSynKeyword` |
| `preprocessor` | `syn-preprocessor` | `\TheSynPreprocessor` |
| `type` | `syn-type` | `\TheSynType` |
| `function` | `syn-function` | `\TheSynFunction` |
| `constant` | `syn-constant` | `\TheSynConstant` |
| `identifier` | `syn-identifier` | `\TheSynIdentifier` |
| `operator` | `syn-operator` | `\TheSynOperator` |
| `punctuation` | `syn-punctuation` | `\TheSynPunctuation` |
| `paren` | `syn-paren` | `\TheSynParen` |

Text whose logical style has no renderer mapping is still escaped and emitted,
but it is not wrapped in a style element or macro. A theme can freely change
the appearance of mapped categories without changing parser behavior.

## Template customization

The wrapper automatically chooses the template tree that matches `--format`:

```text
templates/html/default/
templates/tex/default/
```

In a source checkout these live under
`tools/batch-md-rexx/templates`. In a build-tree release they live under
`release/batch-md-rexx/templates`. In an installation they live under
`<prefix>/share/the/batch-md-rexx/templates`.

A custom directory passed through `--template-dir` must be complete for its
format, even when `--include-style false` is used.

### Required HTML fragment templates

| File | Purpose |
| --- | --- |
| `source-open.tpl` | Opens the optional source wrapper. |
| `source-close.tpl` | Closes the optional source wrapper. |
| `style-token.tpl` | Renders one classified source span. |
| `fragment-style.css` | Supplies optional inline/site CSS for the fragment. |

Default token template:

```html
<span class="{{style_class}}">{{text}}</span>
```

HTML template values:

- `{{style}}`: logical DSLSH/THE style name;
- `{{style_class}}`: stable `syn-*` class;
- `{{text}}`: already HTML-escaped source text.

### Required TeX fragment templates

| File | Purpose |
| --- | --- |
| `source-open.tpl` | Opens the optional TeX block wrapper. |
| `source-close.tpl` | Closes the optional TeX block wrapper. |
| `style-token.tpl` | Renders one classified source span. |
| `style.tex` | Defines colours, `\TheSyn...` macros, and `TheCodeBlock`. |

Default token template:

```tex
\TheSyn{{style_macro}}{{{text}}}
```

TeX template values:

- `{{style}}`: logical DSLSH/THE style name;
- `{{style_macro}}`: suffix such as `Keyword`, `String`, or `Type`;
- `{{text}}`: already TeX-escaped source text.

### Creating a theme

Copy one complete default directory and edit the copy:

```sh
mkdir -p build
cp -R tools/batch-md-rexx/templates/html/default build/crexx-html-theme
cp -R tools/batch-md-rexx/templates/tex/default build/crexx-tex-theme
```

Then select it explicitly:

```sh
the-highlight-source \
  --format html-fragment \
  --template-dir build/crexx-html-theme \
  example.crexx

the-highlight-source \
  --format tex-fragment \
  --template-dir build/crexx-tex-theme \
  example.crexx
```

For HTML, retain stable `syn-*` class names when site code or cached fragments
depend on them. For TeX, retain the `\TheSyn...` macro surface when generated
fragments and book themes are maintained independently. Colours, fonts,
weights, borders, padding, and wrapper markup can otherwise be changed freely.

## Environment and dependency discovery

The wrapper supports explicit paths for reproducible builds and automatic
discovery for normal installations.

| Variable | Default or discovery rule |
| --- | --- |
| `CREXX` | `crexx` on `PATH`. |
| `THE_BIN` | `the` on `PATH`. |
| `THE_HOME_DIR` | Installed `<prefix>/share/the` or the staged `release` directory. Set it explicitly when running the wrapper from the source checkout. |
| `THE_CREXX_RXC` | Explicit value; otherwise `rxc` beside `CREXX`, then `rxc` on `PATH`. |
| `THE_CREXX_RXAS` | Explicit value; otherwise `rxas` beside `rxc`, then `rxas` on `PATH`. |
| `THE_CREXX_CACHE_DIR` | Optional THE-specific CREXXSAA source-cache directory. |
| `CREXXSAA_CACHE_DIR` | Optional generic CREXXSAA cache override; wins over `THE_CREXX_CACHE_DIR`. |
| `CREXXSAA_CACHE_DISABLE` | Set to `1` to compile hosted source through temporary files without the persistent cache. |
| `CREXXSAA_CACHE_REFRESH` | Set to `1` to ignore a valid cache hit and replace it. |
| `CREXXSAA_CACHE_TRACE` | Set to `1` to print cache decisions to standard error. |

`THE_HOME_DIR` must identify a coherent THE runtime tree. Pointing it at an
arbitrary directory containing only templates is insufficient.

CREXXSAA also needs a resolvable, writable cache or temporary location. Keep a
normal `HOME`/platform user-profile and temporary-directory environment, or set
`CREXXSAA_CACHE_DIR` explicitly. A fully stripped process environment can fail
before highlighting with `Unable to resolve CREXXSAA cache paths`.

For a custom uninstalled build-tree executable, `THE_DRIVER_PATH` may be needed
to point THE at the directory containing its driver modules. The staged
`release/the` layout and a normal installation already provide a coherent
driver location.

## Performance and cache behavior

A packaged Release build removes the avoidable per-request compilation and
quadratic work from the normal path:

- the wrapper runs `render-html.rxbin` when it is present, falling back to
  `render-html.crexx` only in a source-only layout;
- every request uses the packaged `highlight-source-profile.the` at one stable
  path, so CREXXSAA can validate and reuse its path-scoped compiled cache;
- THE exports all span records through `STYLESPANSTEXT` with one cREXX
  variable-pool update instead of one update per span; and
- the renderer consumes the ordered records and source lines in one advancing
  pass.

The first request after installation or a source/tool upgrade can still be
slower while CREXXSAA refreshes the hosted-profile cache. Subsequent requests
should be warm-cache calls. Do not generate a new profile pathname for every
request: that defeats the cache even when the profile text is unchanged.

`STYLESPANS` remains available for editor profiles that need the traditional
array (`stylespans.0`, `stylespans.1`, and so on). Batch integrations should
prefer the scalar form:

```rexx
stylespanstext = .string
address the "extract /stylespanstext/" expose stylespanstext
```

The scalar contains the same newline-delimited
`line start-cell cell-count style` records. Both extract forms accept an
optional inclusive `start-line end-line` range.

## Website and reverse-proxy integration

The recommended first web integration treats `the-highlight-source` as a
bounded worker behind a cRexx HTTP service:

1. Accept a UTF-8 request body and an allow-listed language.
2. Enforce a request-size limit before writing anything.
3. Write the body to a fresh temporary file with the matching extension.
4. Run `the-highlight-source --format html-fragment --include-style false
   --language <language> <file>` with an outer process timeout.
5. On status `0`, return stdout as `text/html; charset=utf-8`.
6. On status `1`, return sanitized parser diagnostics as a client error.
7. On any other non-zero status, log stderr and return a service error.
8. Delete the temporary file.
9. Cache successful results by source bytes, language, parser/tool version, and
   theme version.

Do not expose `--parser-command`, `--template-dir`, executable paths, or source
filesystem paths as public request parameters. Fix those values in the service
configuration.

The current implementation launches a cRexx renderer and a bounded THE/DSLSH
batch session for each call. The THE test suite serializes documentation-tool
invocations because concurrent runs of the same renderer/toolchain have
produced an otherwise undiagnosed driver status `255`. The safest first service
should likewise serialize calls per installed tool copy. Add parallel workers
only after isolating their tool/runtime state and measuring stable behavior.

Use `--include-style false` for HTTP responses and ship the selected CSS once
with the website. This keeps responses small and lets the website change its
theme without regenerating source classification.

## Book-generation integration

Generate fragments under the book build directory rather than beside source
files. A deterministic layout makes incremental builds and cleanup simple:

```text
build/the-highlight/
  crexx_programming_guide/examples/hello.crexx.thehl.tex
  crexx_vm_spec/examples/sayx.rxas.thehl.tex
```

Generate one fragment per source file with `--include-style false`, include
`style.tex` once in the book preamble, and use `\input` at each listing site.

An existing book pipeline can retain a fallback to its current listing tool:

```tex
\newcommand{\TheInputListing}[3]{%
  \IfFileExists{#3}{%
    \input{#3}%
  }{%
    \lstinputlisting[language=#1]{#2}%
  }%
}
```

Then a generated/manual TeX source can select the highlighted artifact when it
exists:

```tex
\TheInputListing
  {Rexx}
  {../../examples/hello.crexx}
  {../../build/the-highlight/hello.crexx.thehl.tex}
```

This keeps highlighting an enhancement rather than making a missing generated
fragment fatal to every book build. The fallback assumes the existing book
already loads the TeX `listings` package.

## Diagnostics and troubleshooting

| Symptom or message | Likely cause and action |
| --- | --- |
| `could not locate render-html.crexx` | The wrapper is separated from its source, release, or installed share tree. Restore the packaged layout. |
| `required fragment template not readable` | `--template-dir` is incomplete or points at the wrong format. Supply all four required files. |
| `RXAS parser command not found` | Install/build parser-enabled `rxas`, set `THE_CREXX_RXAS`, or avoid RXAS input. |
| `parser diagnostics present` | The configured parser rejected or diagnosed the source. Read stderr and correct the source/version mismatch. |
| `SDSLHWAIT timed out` on a large valid source | Confirm that THE and the selected parser commands are compatible Release builds containing the current highlighting-performance fixes. Verify the actual `THE_CREXX_RXC` path before merely increasing `--timeout`. No partial fragment is returned. |
| `THE failed ... rc 99` with a CREXXSAA cache error | Provide a writable normal cache/home environment or set `CREXXSAA_CACHE_DIR`. Check the CREXX runtime/import installation. |
| THE reports CREXX unavailable | Rebuild THE with `USE_CREXX=ON` and a compatible CREXXSAA ABI, compiler, assembler, and `library.rxbin`. |
| Output contains source but no styled tokens | Confirm THE was built with `USE_SDSLH=ON` and the parser command supports `--syntaxhighlight`. |
| Intermittent status `255` under parallel load | Serialize calls to one installed renderer/toolchain copy; then investigate isolated worker layouts before enabling parallelism. |
| TeX reports undefined `\TheSyn...` or `TheCodeBlock` | Load the matching `style.tex` once before inputting fragments. |
| TeX reports undefined `\textcolor` | Load `\usepackage{xcolor}`. |

Set `CREXXSAA_CACHE_TRACE=1` when diagnosing hosted-profile cache selection.
Keep stderr separate from generated stdout; stdout is fragment data.

## Validation

The checked-in smoke source is:

```text
tools/batch-md-rexx/examples/dslsh-highlight/hello.crexx
```

Focused validation from a configured THE build:

```sh
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure \
  -R '^test_batch_md_rexx_highlight_source$'
```

The complete documentation-tool group is:

```sh
ctest --test-dir cmake-build-debug --output-on-failure \
  -R '^test_batch_md_rexx_'
```

The focused test covers cRexx and RXAS, TeX with and without definitions and
wrappers, HTML with and without inline CSS, the public wrapper, and release-tree
resource discovery.

For a real TeX compile smoke test, generate a fragment with
`--include-style true`, input it from a minimal document that loads `xcolor`,
and compile it with the same TeX engine used by the books.

## Current boundaries

- The command supports one file per invocation.
- HTML and TeX outputs are fragments, not complete documents.
- Parser diagnostics fail the operation; partial highlighted output is not a
  supported success response.
- RXPP needs its mapped-buffer/parser wrapper and is not accepted by this
  focused command.
- The span interface is based on THE display-cell positions. Test exact
  appearance for source containing complex grapheme clusters before making
  column-sensitive downstream assumptions.
- Process startup and parser time make caching appropriate for a website.
- The full Markdown runner has broader execution and validation features but a
  larger integration surface.

## Related material

The following files are useful background but are not required to operate the
command after reading this guide:

- `tools/batch-md-rexx/examples/dslsh-highlight/README.md`: very short runnable
  example;
- `doc/sdslh.md`: THE editor token/category and theme mapping;
- `doc/batch-md-rexx-format.md`: full Markdown example-runner format;
- `doc/batch-md-rexx-design.md`: implementation story and design history;
- `tools/batch-md-rexx/HANDOVER.md`: manual/Docker integration notes;
- `doc/crexx.md`: THE's embedded CREXX and CREXXSAA configuration.
