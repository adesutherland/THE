# THE (The Hessling Editor) - CREXX Edition

**THE** is a full-screen character-mode text editor based on the VM/CMS editor XEDIT and many features of KEDIT written by Mansfield Software.

### About this Fork (4.0.CREXX)

This repository is a significantly modernized fork of the original [Hessling Editor (v4.0)](https://hessling-editor.sourceforge.net). 

The primary goals of this fork are:
1. **Modern Toolchain:** The build system has been completely rewritten from legacy `make`/`autoconf` scripts to a modern, robust **CMake** implementation.
2. **C99 Compliance:** The entire codebase has been strictly refactored from pre-ANSI (K&R) C to the **C99** standard, resolving hundreds of compiler warnings on modern compilers (Clang/GCC).
3. **CREXX Integration:** This fork is specifically designed to support the integration of [CREXX](https://github.com/crexx-org), bringing a modern, high-performance, and deeply integrated Rexx scripting engine natively into the editor.

## Architecture
For developers and contributors, a high-level overview of the editor's internal design, event loop, and data structures is available in the [Architecture Overview](doc/architecture.md). The CREXX bridge is documented separately in [CREXX Integration](doc/crexx.md).

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
