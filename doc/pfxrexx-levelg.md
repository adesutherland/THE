# Compiled Rexx and HLASM prefix workflows

The bundled `pfxstructuredg.the` is an independently written Level G port of
the PFXREXX and PFXHLASM workflows. It runs with cREXX and does not require
Regina or a download of Shmuel's files. The separately supplied
[minimal patches](private-compatibility.md) let you use the reference macros
with Regina instead.

With the default cREXX engine, enter either:

```text
macro pfxsetupg REXX
macro pfxsetupg HLASM
```

Run one setup at a time: the two workflows share aliases such as DO and IF.
Setup selects XEDIT compatibility, mixed-case entry and a twelve-column prefix
area. Type a prefix beside a record and press Enter. The templates, continuation
markers, case conversion, assembler STAR/UNSTAR, counts from 1 through 99 and
cursor priorities and default indentation follow the patched reference. CMS
GLOBALV overrides are not provided. New pending markers wait for the next
attention event, so typing over a placeholder and pressing Enter continues input.

The port uses typed Level G procedures and objects, compiled by the bundled
cREXX compiler and executed through native ADDRESS THE. It is not Classic Rexx
source and cannot run under Regina. Choose `THE_REXX_INTERPRETER=REGINA` and
the patched originals when that is preferred.

The behavior suite compares 121 workflows between the actual patched Regina
files and the compiled port: saved text, cursor state, pending commands and
error markings. It covers all 57 original aliases, counts, label/indentation
contexts, CASE/CASENTRY, condition codes and typed continuation input. A real
curses keyboard test also checks saved bytes. Both paths reject invalid counts
without changing the user's records. This establishes the tested fork behavior;
it does not establish every possible macro operand, HLASM program correctness,
upstream THE behavior or equivalence to a live XEDIT system. Windows interactive
qualification remains pending Adrian's manual test.

The comparison also found two additional reference-source defects after the
first Regina pre-release: numeric search positions used as Boolean expressions
in label and CASENTRY/IF contexts. The patch kit now uses explicit comparisons,
and regression cases cover those paths. Original download hashes are unchanged.
