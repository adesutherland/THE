# Macro interpreters and prebuilt packages

CRXTHE 0.10.dev is an independent fork based on Mark Hessling's THE 4.0. Its changes are not fixes for
upstream THE. cREXX is the default macro interpreter; Regina and ooRexx are optional.
Select one interpreter before starting THE. Switching interpreters requires
restarting the editor; macros are not automatically translated between dialects.

The bundled package supplies cREXX's shared runtime, compiler, assembler,
runner, matching `library.rxbin` and compiler exit module privately. It does not change an existing
cREXX installation. The installers offer bundled cREXX (default) or an
installed cREXX when compatible. The bundle is always retained for automatic
fallback. Launch `bin/crxthe` (`bin/crxthe.cmd` on Windows); its paths are
relative to the package, so a portable archive can be moved or unpacked into a
directory containing spaces.

The installed-runtime preference is rechecked at startup. THE looks for a
complete tool set using `CREXX_HOME`, the Windows cREXX installation registry,
PATH and standard Unix locations. It validates the required hosting API and
compiles/runs a small probe in a separate process. A missing, incomplete or
incompatible installation causes an informational message and automatic use
of the bundled copy; no question is asked. Changing or removing a standalone
installation therefore does not require reinstalling THE.

`THE_CREXX_RUNTIME=BUNDLED|INSTALLED` overrides the saved installer preference.
With `INSTALLED`, `THE_CREXX_HOME` can specify the installation root or its
`bin` directory. If that explicit location is unusable, THE falls back to its
bundle rather than selecting a different installation. Portable ZIPs default
to bundled cREXX and support the same environment settings. Runtime selection
is separate from `THE_REXX_INTERPRETER`, which chooses CREXX, REGINA, OOREXX or NONE.

THE uses its own cREXX source cache by default, including when it selects an
installed cREXX:

- Windows: `%LOCALAPPDATA%\THE-cREXX\Cache\crexxsaa`.
- macOS: `~/Library/Caches/org.crexx.the/crexxsaa`.
- Linux: `$XDG_CACHE_HOME/the/crexxsaa`, or `~/.cache/the/crexxsaa`.

Set `THE_CREXX_CACHE_DIR` to override that location. The existing generic
`CREXXSAA_CACHE_DIR` override retains precedence when both are set. These
settings affect THE and its children; no user/system environment setting or
standalone cREXX cache is changed. Cache reuse still checks source content,
namespace and compiler/library configuration. Switching tool sets invalidates
the relevant cached object automatically. Deleting THE's cache forces a
recompile; the selected cREXX installation stays the same.

THE loads only the selected engine at runtime. The build uses `crexxsaa.h`
(ABI 3 or newer) and, when Classic bridges are enabled, Regina's `rexxsaa.h`
or ooRexx's `rexx.h`. Users do not
need those headers, a compiler, or a rebuild. A missing or incompatible library
produces a diagnostic and disables macros; THE does not silently switch engines.
Startup profiles are skipped when the engine is unavailable; Rexx text is never
reinterpreted as editor/OS commands in that case. The editor remains usable.

## Optional Regina setup

Install Regina yourself, for the same CPU architecture as THE. Obtain it from
[the Regina project](https://regina-rexx.sourceforge.io/). On macOS, an ARM64
THE needs an ARM64 Regina library, and an Intel THE needs an Intel library.
Regina is not included in the THE package.

For macOS or Linux, configure the engine and its **full library path**:

```sh
export THE_REXX_INTERPRETER=REGINA
export THE_REGINA_LIBRARY=/full/path/to/libregina.dylib  # macOS
# export THE_REGINA_LIBRARY=/full/path/to/libregina.so  # Linux
the myfile.rexx
```

For example, a Homebrew installation's path can be obtained with
`brew --prefix regina-rexx`; append `/lib/libregina.dylib`. The loader also
accepts the platform library name through the OS search mechanism, but an
explicit path avoids depending on that mechanism.

In Windows PowerShell:

```powershell
$env:THE_REXX_INTERPRETER = 'REGINA'
$env:THE_REGINA_LIBRARY = 'C:\path\to\Regina\regina.dll'
& "$env:LOCALAPPDATA\Programs\THE-cREXX\bin\crxthe.cmd" myfile.rexx
```

The full launcher path selects this fork even if upstream THE is also on PATH.
For a portable ZIP or a custom install directory, use its `bin\crxthe.cmd` instead.

Regina's own dependent libraries must also be available to the OS loader.
These settings affect this terminal and child processes. Put them in your shell
profile, or set user environment variables, to persist them.

THE selects `profile_regina.the` and Classic Rexx variants of its system
profiles. An explicit `-p`, `THE_PROFILE_FILE`, or personal Unix `.therc` takes
precedence: ensure that profile is written for Regina. cREXX Level B profiles
are not Classic Rexx. To return to the bundled default, unset the two variables
or set `THE_REXX_INTERPRETER=CREXX`.

## Optional ooRexx macro engine

Install ooRexx yourself for the same CPU architecture as THE. The current
CRXTHE package does not include it. A build must have been configured with
`THE_ENABLE_OOREXX=ON` and matching ooRexx headers. Then select it before
starting the editor and provide the full path to its shared library:

```sh
export THE_REXX_INTERPRETER=OOREXX
export THE_OOREXX_LIBRARY=/full/path/to/librexx.dylib  # macOS
# export THE_OOREXX_LIBRARY=/full/path/to/librexx.so  # Linux
the myfile.rexx
```

In Windows PowerShell, after installing a matching Windows x64 ooRexx:

```powershell
$env:THE_REXX_INTERPRETER = 'OOREXX'
$env:THE_OOREXX_LIBRARY = 'C:\path\to\ooRexx\rexx.dll'
& "$env:LOCALAPPDATA\Programs\THE-cREXX\bin\crxthe.cmd" myfile.rexx
```

The selected `rexx` library and its companion `rexxapi` library must load in
the current process. By default THE looks beside `THE_OOREXX_LIBRARY` and then
through the platform library search path. Set `THE_OOREXX_API_LIBRARY` to the
companion's full path when it is elsewhere (`librexxapi.dylib`,
`librexxapi.so` or `rexxapi.dll`).
The default and trusted browser profiles use the shared Classic Rexx variants
(`profile_regina.the` and `web-profile-regina.the`) with ooRexx, including
their `_regina` helper macros. An explicit personal profile takes precedence;
use Classic Rexx syntax there. cREXX Level B source and `.rxbin` bytecode are
not ooRexx macros. If the bridge was not compiled or the library is absent or
incompatible, THE reports the problem and leaves macros unavailable instead of
switching engines. Clear the ooRexx variables to return to the cREXX
default.

This in-process macro engine is distinct from the [native ooRexx source-tool
runner](workbench-native-tools.md). A WORKBENCH TOOL with an explicitly chosen
ooRexx executable starts a separate program with a source snapshot; selecting
the macro engine neither creates nor selects that tool.

Use `THE_REXX_INTERPRETER=NONE` for an editor without macro support. Advanced
cREXX overrides are `THE_CREXX_LIBRARY`, `THE_CREXX_LOCATION`,
`THE_CREXX_RXC`, `THE_CREXX_RXAS`, `THE_CREXX_IMPORT_DIR`,
`THE_CREXX_LIBRARY_RXBIN` and `THE_CREXX_CACHE_DIR`. Keep tools and bytecode
from the same cREXX build. These settings are read when THE starts.

## cREXX extras

The standard profile uses the bundled `rxc` and `rxas` for compiler-backed
highlighting. See [first launch](first-launch.md) and the Windows
[installation guide](Windows-Readme.txt) for normal use. The optional F8/F9
compile/run development helpers have unresolved failures and are not enabled
by the stock profile.

Shmuel Metz's prefix macros, adaptations, cREXX ports and patches are all kept
in a [private kit](private-compatibility.md), outside new public packages.

The [panel examples](panels.md) use `macro paneldemo` for cREXX and
`macro paneldemo_regina` for Regina or ooRexx. They use shared
protected/editable fields and tagged READ results, with the documented queue
adaptation for cREXX.

## Building

`-DTHE_REXX_INTERPRETER=CREXX|REGINA|OOREXX|NONE` sets the build's default.
`USE_CREXX` preserves its previous role as the initial default choice.
`-DTHE_ENABLE_REGINA=ON` detects Regina headers and compiles its optional
bridge without linking Regina. Selecting a Regina default requires its headers;
otherwise their absence disables the optional bridge. Set `REGINA_INCLUDE_DIR`
or `REGINA_HOME` if necessary. Runtime engine selection is through the
environment variable with the same name. `-DTHE_ENABLE_REGINA=OFF` omits that
bridge. `-DTHE_ENABLE_OOREXX=ON` enables independent ooRexx header discovery;
`OOREXX_HOME` or `OOREXX_INCLUDE_DIR` can point to a matching SDK, and
`OOREXX_LIBRARY` and `OOREXX_API_LIBRARY` can identify its two libraries for
optional real-engine tests.
Selecting OOREXX as the build default requires its headers. The browser proof
of concept selects a Classic variant of its trusted profile for Regina and
ooRexx. Its existing restrictions on client-issued macros/OS commands still
apply.

For a relocatable bundle, configure with `-DTHE_BUNDLE_CREXX=ON` and the default
cREXX interpreter. A matching shared cREXX runtime and tool set are required;
the install step stages them privately. See [packaging](packaging.md)
for archive and installer commands and qualification limits.
