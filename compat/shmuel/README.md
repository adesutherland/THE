# Regina compatibility patches for Shmuel Metz's macros

These patches apply to the PFXREXX and PFXHLASM THE files downloaded from
[Shmuel Metz's source collection](https://mason.gmu.edu/~smetz3/#professional).
THE distributes its corrections separately. The original files are not
included or overwritten. The preparation script verifies their exact SHA-256
hashes before applying any change and refuses to overwrite edited output.

With Python 3, from the source checkout:

```sh
python3 scripts/prepare-shmuel-macros.py --download --output-dir /path/to/my-macros
```

Alternatively supply `--source-dir /path/to/unchanged-downloads`. In an
installed package the kit is under `share/the/macro-patches/`; run its
`scripts/prepare-shmuel-macros.py`. No compiler or headers are needed.
The download requires network access; applying patches to existing downloads
does not. Output includes patched files, a setup macro and provenance hashes.

Configure THE to use Regina as described in
[interpreter setup](../../doc/rexx-interpreters.md) (installed as
`share/doc/the/rexx-interpreters.md`), set `THE_MACRO_PATH` to the
output directory, then run `macro pfxsetup-original REXX` or
`macro pfxsetup-original HLASM` inside THE. The setup installs the selected
package's upper/lower-case aliases. Run the appropriate setup again when
switching between Rexx and HLASM workflows: names such as DO overlap.

Corrections are limited to the experimental fork's tested workflow:

- Both: early CLEAR handling, bounded counts, `datatype` typo, recognizing
  NEW/NEW CHANGED records, safe errors and avoiding invalid negative indentation.
- PFXREXX: compare against the actual THE placeholder, insert blank records
  explicitly, restore state on error, correct OTHERWISE and upper-case column
  conversion, and isolate case-conversion variables.
- Label and CASENTRY/IF searches: compare numeric positions explicitly rather
  than interpreting positions greater than one as Boolean values.
- PFXHLASM: repair malformed comments/control flow, initialize indentation,
  use the configured SEARCH indentation, restore settings and cursor handling,
  correct WHEN to assembler syntax, supply documented but missing ENDIF,
  ITERATE, ORELSE and OTHRWISE templates, and adapt STAR/UNSTAR to THE's
  column-overlay commands. Interactive debug tracing defaults off.

The editor separately implements TRUNC state/query, preservation and basic
non-spilling truncation. Full XEDIT SPILL semantics are outside this stage.

QA runs the actual patched files: all 57 documented aliases, template counts,
continuation cleanup, invalid-count errors, case-conversion boundaries,
STAR/UNSTAR, Rexx DO operands and real text entry. The native curses test
checks saved bytes. Windows CI runs the same behavior suite against both the
build and the installed candidate. No live XEDIT or HLASM assembly/execution
comparison is claimed. Interactive Windows qualification remains pending.
