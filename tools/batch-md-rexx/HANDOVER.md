# Batch Markdown REXX Handover

This note is for the next integration step: using THE/SDSLH syntax highlighting in the CREXX manual generation pipeline. The work should be done in a VM with Docker capability and should keep the CREXX document build fallback-friendly.

## Current THE-side state

THE now provides two related tools:

- `the-batch-md-rexx`: full Markdown example renderer. It can scan fenced REXX examples, highlight source, run examples, validate output, and render HTML/TeX/PDF.
- `the-highlight-source`: focused source highlighter for documentation pipelines. It reads one source file and emits an HTML or TeX fragment using the same THE/SDSLH style spans.

For the CREXX manual request, prefer `the-highlight-source`. It is deliberately smaller than the full batch renderer and does not replace the existing CREXX book pipeline.

Example:

```sh
the-highlight-source docs/books/crexx_programming_guide/examples/hello.rxas \
  > build/the-highlight/hello.rxas.thehl.tex

the-highlight-source --format html-fragment \
  docs/books/crexx_programming_guide/examples/hello.crexx \
  > build/the-highlight/hello.crexx.thehl.html
```

Useful options:

```sh
--format html-fragment|tex-fragment
--language rexx|crexx|the|rxas
--include-style true|false
--include-wrapper true|false
```

Defaults:

- Language is inferred from `.rexx`, `.crexx`, `.the`, or `.rxas`.
- TeX fragment output remains the default; HTML is selected explicitly.
- Style definitions are not emitted by default.
- TeX output is wrapped in `\begin{TheCodeBlock}` / `\end{TheCodeBlock}`;
  HTML output is wrapped in `<pre class="the-example-source"><code>`.

For a standalone smoke test, use `--include-style true`. For a generated
website or manual, keep the default `--include-style false` and include tuned
style definitions once. The canonical end-user description of capabilities,
dependencies, command syntax, templates, category mapping, HTML/TeX use, and
the reverse-proxy contract is `doc/syntax-highlighting.md` in the THE source
tree. It is packaged as `doc/syntax-highlighting.md` in a staged release and as
`share/doc/the/syntax-highlighting.md` in an installation. The example README
is intentionally only a quick start.

## Environment expected by the wrapper

The wrapper auto-discovers common installed paths, but Docker jobs should set these explicitly:

```sh
export CREXX=/path/to/crexx
export THE_BIN=/path/to/the
export THE_HOME_DIR=/path/to/the-share-or-release-dir
export THE_CREXX_RXC=/path/to/rxc
export THE_CREXX_RXAS=/path/to/rxas
```

`THE_HOME_DIR` must contain THE runtime resources, including drivers and syntax resources. In a build-tree smoke test this is usually `cmake-build-debug/release`; in an installed image it is normally the installed `share/the` location.

## Proposed CREXX Docker integration

Do this surgically. Do not convert the CREXX manuals to the full batch Markdown renderer yet.

1. Build or install CREXX in the doc image.
2. Build or install THE with SDSLH and CREXX support in the same image.
3. Install TeX tooling for the existing manual build. `tectonic` is the simplest smoke-test engine; use `latexmk`/XeLaTeX only if the existing books require the broader TeX stack.
4. Add a small CREXX docs prebuild step that generates highlighted fragments for selected examples.
5. Add a LaTeX fallback macro so manuals still build when highlighted fragments are absent.
6. Switch one low-risk generated area first, preferably VM instruction RXAS listings emitted by `docs/books/crexx_vm_spec/instruction_doc.rexx`.
7. Extend to named Markdown examples only after the RXAS path is stable.

## Suggested generated-file layout

Avoid writing generated fragments into the CREXX source tree during Docker builds. Put them under the document build directory.

Example shape:

```text
build/
  the-highlight/
    crexx_programming_guide/examples/hello.crexx.thehl.tex
    crexx_programming_guide/examples/hello.rxas.thehl.tex
    crexx_vm_spec/examples/sayxREG.rxas.thehl.tex
```

A simple generator can walk:

```sh
docs/books/*/examples/*.crexx
docs/books/*/examples/*.rexx
docs/books/*/examples/*.rxas
```

and run:

```sh
the-highlight-source "$source_file" > "$generated_fragment"
```

Keep the generated path deterministic and relative to the TeX build directory so the LaTeX fallback macro can use it reliably.

## Suggested LaTeX contract

Include the THE style definitions once in the book preamble. The first pass can either input THE's default `style.tex` or copy the small macro/color block into a CREXX-owned `thehighlight.tex`.

Use a fallback macro with both the original source path and generated fragment path:

```tex
\newcommand{\TheInputListing}[3]{%
  \IfFileExists{#3}{%
    \input{#3}%
  }{%
    \lstinputlisting[language=#1]{#2}%
  }%
}
```

Then generated CREXX TeX can say:

```tex
\TheInputListing{rxas}{../../examples/sayxREG.rxas}{../../build/the-highlight/crexx_vm_spec/examples/sayxREG.rxas.thehl.tex}
```

This keeps the current `listings` renderer as the fallback and makes THE highlighting an enhancement rather than a hard dependency.

## Validation checklist

Run these in THE before handing the tool to the CREXX Docker job:

```sh
cmake --build cmake-build-debug --target the
ctest --test-dir cmake-build-debug --output-on-failure -R 'test_batch_md_rexx_'
```

Run a release-layout source fragment smoke test:

```sh
cmake-build-debug/release/the-highlight-source \
  tools/batch-md-rexx/tests/fixtures/crexx-doc-hello.rxas \
  > /tmp/hello.rxas.thehl.tex
```

Optional compile smoke test:

```sh
cmake-build-debug/release/the-highlight-source --include-style true \
  tools/batch-md-rexx/tests/fixtures/crexx-doc-hello.rxas \
  > /tmp/fragment.tex
```

Then input `/tmp/fragment.tex` from a tiny document using `\usepackage{xcolor}` and compile it with `tectonic`.

In CREXX, validate both modes:

- THE disabled or fragments absent: manuals still build using `\lstinputlisting`.
- THE enabled and fragments generated: manuals use `\input{*.thehl.tex}`.

## Known operational notes

- RXAS highlighting needs an RXAS parser command. In Docker set `THE_CREXX_RXAS` explicitly.
- Some RXAS parser runs may create `rxas_parser.log` in the current directory. Run the generator from a build directory or clean that file as part of the docs job.
- Keep THE highlighting optional until the Docker document image owns THE, CREXX, and TeX dependencies.
- The full `the-batch-md-rexx` renderer remains useful for tutorial/demo validation, but it is intentionally not the first integration point for CREXX manuals.
