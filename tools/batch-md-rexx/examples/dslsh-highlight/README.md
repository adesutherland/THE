# DSLSH highlighting quick start

This example sends one cRexx source file through the cRexx parser, DSLSH, and
THE, then renders the same classified spans as either HTML or TeX.

The canonical end-user guide is `doc/syntax-highlighting.md` in the THE source
tree. It documents capabilities, dependencies, every command option, templates,
theme customization, website/reverse-proxy use, TeX/book use, deployment, and
troubleshooting. A staged release contains it at
`doc/syntax-highlighting.md`; an installation contains it at
`share/doc/the/syntax-highlighting.md`.

With sibling Debug builds, run from the THE repository root:

```sh
export CREXX="$PWD/../CREXX/cmake-build-debug/bin/crexx"
export THE_CREXX_RXC="$PWD/../CREXX/cmake-build-debug/bin/rxc"
export THE_CREXX_RXAS="$PWD/../CREXX/cmake-build-debug/bin/rxas"
export THE_BIN="$PWD/cmake-build-debug/release/the"
export THE_HOME_DIR="$PWD/cmake-build-debug/release"
```

Generate a self-styled HTML sample:

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
  tools/batch-md-rexx/examples/dslsh-highlight/hello.crexx \
  > /tmp/hello.crexx.thehl.tex
```

For production, include the chosen CSS or `style.tex` once, leave
`--include-style` false for each fragment, and tune a copied template directory
as described in the user guide.
