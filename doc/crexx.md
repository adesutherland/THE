# CREXX integration

This fork can run THE profiles and macros through CREXX. THE still keeps its
longstanding command subsystem: CREXX programs enter through a small
`crexxsaa` host layer, select the `THE` ADDRESS environment, and issue normal
THE commands.

## Build discovery

CREXX support is controlled by the CMake `USE_CREXX` option, which defaults to
`ON`.

At configure time THE looks for:

- `libcrexxsaa`
- `crexxsaa.h` with `CREXXSAA_ABI_VERSION` 3 or newer
- `rxc`
- `rxas`
- `library.rxbin`

The search checks the sibling CREXX build directories first and then
`$HOME/.local/bin` / `$HOME/.local/include`. If these pieces are missing, THE
builds without the CREXX bridge.

## Runtime model

When CREXX support is available:

- THE initializes a `crexxsaa_context`
- THE registers a native `ADDRESS THE` environment
- CREXX source profiles and macros are compiled and cached by `crexxsaa`
- existing `.rxbin` macros can run directly
- CREXX commands addressed to `THE` are passed to THE's normal command parser

Direct `.rxbin` macros must be rebuilt with the current CREXX toolchain after
CREXX bytecode format changes. The descriptor-based lookup change uses format
006 and is exposed through `CREXXSAA_ABI_VERSION` 3.

Hosted profiles and macros should declare their own language level and command
environment:

```rexx
options levelb
address the
'msg hello from CREXX'
```

`crexxsaa` does not add `OPTIONS`, `ADDRESS`, imports, or compatibility
preludes. Source files must be valid CREXX source as written.

## Configuration

THE forwards these host-specific environment variables to `crexxsaa`:

- `THE_CREXX_RXC`
- `THE_CREXX_RXAS`
- `THE_CREXX_IMPORT_DIR`
- `THE_CREXX_LOCATION`
- `THE_CREXX_LIBRARY_RXBIN`
- `THE_CREXX_CACHE_DIR`

The generic `crexxsaa` variables are also supported:

- `CREXXSAA_CACHE_DIR`
- `CREXXSAA_CACHE_DISABLE=1`
- `CREXXSAA_CACHE_REFRESH=1`
- `CREXXSAA_CACHE_TRACE=1`
- `CREXXSAA_RXC`
- `CREXXSAA_RXAS`
- `CREXXSAA_IMPORT_DIR`

`CREXXSAA_CACHE_DIR` wins over `THE_CREXX_CACHE_DIR` when both are set.

The `crexxsaa` maintenance tool can be used to inspect or clear the compiled
profile/macro cache:

```sh
crexxsaa --location
crexxsaa --list
crexxsaa --clear
```

## Variables

THE's existing Rexx-facing variable helpers are mapped onto the `crexxsaa`
ADDRESS variable facade. CREXX scripts can use either direct exposure or a
sandbox.

Direct array exposure:

```rexx
filename = .string[]
address the "extract /filename/" expose filename[]
```

Direct scalar exposure:

```rexx
the_value = "example"
address the "editv put the_value" expose the_value
```

Sandbox exposure:

```rexx
pool = .standardaddresssandbox()
address the "extract /filename/" sandbox pool
```

For compatibility with THE's historical stem conventions, a directly exposed
scalar also has a one-item stem view: `name.0` reads as `1`, `name.1` reads the
scalar, writes to `name.0` are ignored, and writes to `name.1` update the
scalar. Real exposed arrays keep their normal array semantics.

## Commands for migrated macros

CREXX-hosted macros call THE commands through ADDRESS. The integration does not
bridge old Rexx external functions directly. Where migrated macros need
function-like editor capabilities, add explicit THE commands instead.

The first such command is `VALIDTARGET`, which exposes the old target parser to
CREXX scripts:

```rexx
validtarget = .string[]
address the "validtarget spare 1 /tail/" expose validtarget[]
```

See [VALIDTARGET](commands/VALIDTARGET.md) for command details.

## Profiles

The default `profile.the` is valid CREXX source and configures the editor for
CREXX syntax highlighting through SDSLH.

`profile_crexx.the` is an optional CREXX-focused profile for daily CREXX work.
It keeps the default editing/theme setup and adds convenience bindings:

- `F8`: compile the current file with `crexx -noexec`
- `F9`: save and run the current file with `crexx`
- `F10`: list the `crexxsaa` source cache
- `F11`: print the `crexxsaa` cache location

Use it explicitly:

```sh
the -p ~/.local/share/the/profile_crexx.the myprog.rexx
```

The helper macros behind `F8` and `F9` are `crexxcompile.the` and
`crexxrun.the`. They are intended for interactive editor sessions; THE batch
mode rejects `OS` commands, so batch tests should call the CREXX toolchain
directly rather than using those key helpers. Both macros extract the active
file's full editor id before invoking `crexx`, so switching files in the ring
changes the compile/run target.

## Batch smoke testing

THE can be exercised in batch mode with a CREXX profile:

```sh
THE_CREXX_RXC=~/.local/bin/rxc \
THE_CREXX_RXAS=~/.local/bin/rxas \
THE_CREXX_IMPORT_DIR=~/.local/bin \
THE_CREXX_LOCATION=~/.local/bin \
THE_CREXX_LIBRARY_RXBIN=~/.local/bin/library.rxbin \
CREXXSAA_CACHE_TRACE=1 \
the -b -q -p tests/crexx_profile.the sample.txt
```

This is the shape used by the automated CREXX profile test.
