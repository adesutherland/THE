# Macro interpreters and prebuilt packages

This is the experimental THE cREXX fork. Its changes are not fixes for
upstream THE. cREXX is the default macro interpreter; Regina is optional.
Select one interpreter before starting THE. Switching interpreters requires
restarting the editor; macros are not automatically translated between dialects.

The bundled package supplies cREXX's shared runtime, compiler, assembler,
runner and matching `library.rxbin` privately. It does not change an existing
cREXX installation. Launch `bin/the` (`bin/the.cmd` on Windows); its paths are
relative to the package, so a portable archive can be moved or unpacked into a
directory containing spaces.

THE loads only the selected engine at runtime. The build uses `crexxsaa.h`
(ABI 3 or newer) and, when Regina support is enabled, `rexxsaa.h`. Users do not
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
the myfile.rexx
```

Regina's own dependent libraries must also be available to the OS loader.
These settings affect this terminal and child processes. Put them in your shell
profile, or set user environment variables, to persist them.

THE selects `profile_regina.the` and Classic Rexx variants of its system
profiles. An explicit `-p`, `THE_PROFILE_FILE`, or personal Unix `.therc` takes
precedence: ensure that profile is written for Regina. cREXX Level B profiles
are not Classic Rexx. To return to the bundled default, unset the two variables
or set `THE_REXX_INTERPRETER=CREXX`.

Use `THE_REXX_INTERPRETER=NONE` for an editor without macro support. Advanced
cREXX overrides are `THE_CREXX_LIBRARY`, `THE_CREXX_LOCATION`,
`THE_CREXX_RXC`, `THE_CREXX_RXAS`, `THE_CREXX_IMPORT_DIR`,
`THE_CREXX_LIBRARY_RXBIN` and `THE_CREXX_CACHE_DIR`. Keep tools and bytecode
from the same cREXX build. These settings are read when THE starts.

## cREXX extras

The standard profile uses `rxc` and `rxas` on the package's private PATH for
compiler-backed highlighting. `the -p profile_crexx.the file.crexx` also binds
F8 to compile and F9 to run. These extras use cREXX; the shared prefix-command
compatibility fixes apply with either engine. The Classic template adaptation
described in [PFXREXX](pfxrexx.md) currently requires Regina.

## Building

`-DTHE_REXX_INTERPRETER=CREXX|REGINA|NONE` sets the build's default.
`USE_CREXX` preserves its previous role as the initial default choice.
`-DTHE_ENABLE_REGINA=ON` detects Regina headers and compiles its optional
bridge without linking Regina. Selecting a Regina default requires its headers;
otherwise their absence disables the optional bridge. Set `REGINA_INCLUDE_DIR`
or `REGINA_HOME` if necessary. Runtime engine selection is through the
environment variable with the same name. `-DTHE_ENABLE_REGINA=OFF` omits that
bridge. No claim is made for running the browser proof of concept with Regina.

For a relocatable bundle, configure with `-DTHE_BUNDLE_CREXX=ON` and the default
cREXX interpreter. A matching shared cREXX runtime and tool set are required;
the install step stages them privately. See [packaging](../packaging/README.md)
for archive and installer commands and qualification limits.
