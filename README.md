# THE (The Hessling Editor) - CREXX Edition

**THE** is a full-screen character-mode text editor based on the VM/CMS editor XEDIT and many features of KEDIT written by Mansfield Software.

### About this Fork (4.0.CREXX)

This repository is a significantly modernized fork of the original [Hessling Editor (v4.0)](https://hessling-editor.sourceforge.net). 

The primary goals of this fork are:
1. **Modern Toolchain:** The build system has been rewritten from legacy `make`/`autoconf` scripts to a modern **CMake** implementation.
2. **C99 Compliance:** The codebase has been refactored from pre-ANSI (K&R) C to the **C99** standard for modern compilers.
3. **Driver Split:** The editor core is separated from physical UI drivers. The default curses UI and the no-curses LLM UI are runtime-loaded driver modules.
4. **CREXX Integration:** This fork supports [CREXX](https://github.com/crexx-org), bringing a modern Rexx scripting engine into the editor.

Supported source platforms are macOS, Linux/POSIX, and native Windows.
Historical DOS, OS/2, VMS, Amiga, BeOS, QNX, DJGPP/GO32, and ancient compiler
source branches have been retired.

## Architecture
For developers and contributors, a high-level overview of the editor's runtime,
driver modules, and data structures is available in the
[Architecture Overview](doc/architecture.md). The driver ownership contract is
tracked in [Cursor Driver Architecture](doc/cursor-driver-architecture.md), and
the CREXX bridge is documented separately in [CREXX Integration](doc/crexx.md).

---

## Building and Installing

The project is natively supported by modern IDEs (like CLion) and easily builds via the standard CMake workflow:

```bash
mkdir -p cmake-build
cd cmake-build
cmake ..
make
make install
```

By default, the `install` target will stage the `the` executable and the necessary configuration files (like `THE_Help.txt`, `syntax/` highlights, `profile.the`, and the optional CREXX helper profile/macros) into a standard release directory structure.

On Windows, the default install prefix is `%USERPROFILE%\.local`, matching the
local CREXX and DSLSH installs. After installing, configure the current
PowerShell session with:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\tools\the-env.ps1
```

To persist the same setup for new terminals and IDE launches:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\the-env.ps1 -PersistUser
```

The script adds `%USERPROFILE%\.local\bin` to `Path` and sets THE/CREXX runtime
variables including `THE_HOME_DIR`, `THE_DRIVER_PATH`, `CREXX_HOME`, and the
`THE_CREXX_*` tool paths. The install target also copies the script to
`%USERPROFILE%\.local\bin\the-env.ps1`. Restart CLion after persisting the
environment if it was already running.

The default UI is curses:

```bash
./cmake-build/the --driver curses path/to/file.txt
```

The no-curses LLM UI uses the real editor runtime:

```bash
./cmake-build/the --driver llm -n path/to/file.txt
```

### DSL Syntax Highlighter Dependency

When `USE_SDSLH` is enabled, THE builds only the DSLSH editor middleware from
`DSL-Syntax-Highlighter/codebuffer`. It does not build DSLSH parser adapters.

If `../DSL-Syntax-Highlighter` exists, CMake uses that local checkout. If not,
CMake fetches DSLSH from `DSLSH_GIT_REPOSITORY` at `DSLSH_GIT_TAG`.

```bash
# Follow the configured branch, currently develop.
cmake -S . -B cmake-build

# Pin to a stable tag or exact WIP commit.
cmake -S . -B cmake-build -DDSLSH_GIT_TAG=<tag-or-commit-sha>

# Ignore a sibling checkout and fetch the configured ref.
cmake -S . -B cmake-build -DDSLSH_PREFER_LOCAL=OFF
```

Use a branch name for easy "latest DSLSH" development. Use a tag or commit SHA
when THE needs a reproducible DSLSH version.

## Quick Tips for macOS Users

By default, THE utilizes the `Home` key to toggle between the command line and the file editing area. However, the `Home` key behaves inconsistently across different Mac terminal emulators.

To resolve this, the included `profile.the` automatically binds **F4** and **Shift-Tab** to the `SOS PREFIX` and `SOS TABB` commands. These bindings will quickly jump your cursor into the prefix area or command line respectively.

## Original Author

Mark Hessling (mark@rexx.org)
[The Hessling Editor SourceForge](https://hessling-editor.sourceforge.net)
