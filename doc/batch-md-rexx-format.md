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
