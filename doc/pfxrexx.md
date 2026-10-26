# PFXREXX template adaptation

Historical guide: this macro package and its ports are now held in the
[private kit](private-compatibility.md), outside the public source and installers.
The setup commands below require that separately supplied kit.

`pfxrexx.the` is a small THE-native implementation of the structured-input
workflow described by Shmuel Metz. It is not an unchanged copy of his package,
an XEDIT emulator, or an upstream THE fix. His
[current macros and documentation](https://mason.gmu.edu/~smetz3/#professional)
and [THE feature request 76](https://sourceforge.net/p/hessling-editor/feature-requests/76/)
are the references. The original package remains separately testable.

Start THE with Regina, then enter `macro pfxsetup` on the command line.
This selects XEDIT compatibility, mixed-case text entry and an eight-column
prefix area. It installs upper- and lower-case aliases for:

| Prefix | Effect |
|---|---|
| `DO`, `DO FOREVER` | Insert a DO/body/END template |
| `IF` | Insert IF/THEN/ELSE templates |
| `SEL`, `SELE`, `SELEC`, `SELECT` | Insert SELECT/WHEN/OTHERWISE templates |
| `WHEN` | Insert a WHEN/body/END template |
| `RI`, `RIC` | Continue indented input; remove an untouched new RIC placeholder |
| `..` | Remove an untouched new dot placeholder |

A leading or trailing count repeats a template or requests that many input
lines; counts are limited to 1–99. Operands other than these and `DO FOREVER`
are rejected with `SET PENDING ERROR`. This first version does not implement
the original package's case-conversion commands, HLASM templates, arbitrary DO
expressions or CMS `GLOBALV`. The core separately supports `MASK`, `FLSCREEN`
and cursor priorities; this small adaptation does not require those facilities.

Type a prefix and press Enter. Newly queued prefix commands wait for the next
attention event. Type over the body dot, then press Enter to create the next
indented placeholder; press Enter on an untouched RIC placeholder to remove it.
The macro positions the cursor with THE's `CURSOR GOTO` command. XEDIT Enter
preserves a prefix macro's resulting cursor instead of forcing the command line.

Session indentation settings use THE's `EDITV` store, for example:

```text
editv set pfxrexx_indent_do 3
editv set pfxrexx_indent_if 3
editv set pfxrexx_indent_select 1
editv set pfxrexx_indent_when 2
```

Values must be whole numbers from 0 to 40; invalid/missing settings use the
defaults above. Put settings in a Regina profile to apply them at startup.

The regression tests exercise both interpreters' pending-command behaviour and
Regina's DO text-entry workflow, IF/SELECT/WHEN templates, continuation cleanup,
counts, indentation settings, error display and installed Classic profiles.
This is local fork qualification, not proof against a live XEDIT system or a
test of every macro in Shmuel's package.
