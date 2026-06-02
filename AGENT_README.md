# THE (The Hessling Editor) - Agent Project Overview

## 1. Current Status (v4.0.CREXX Fork)
**THE** is a full-screen, character-mode text editor based on the VM/CMS editor XEDIT and KEDIT, originally written by Mark Hessling. This repository represents a significantly modernized fork.

### Recent Modernizations:
- **Build System**: The legacy GNU Autotools (`configure`) and scattered platform makefiles have been replaced with a unified **CMake** build system (`CMakeLists.txt`).
- **IDE Support**: The project is now natively supported by modern C/C++ IDEs like CLion out-of-the-box.
- **Codebase Standardization**: The C codebase has been refactored from pre-ANSI (K&R) C to the **C99 standard**. Legacy `#ifdef HAVE_PROTO` blocks and `Args()` macros were stripped.
- **Supported Platforms**: Maintained source targets are macOS, Linux/POSIX, and native Windows. Historical DOS, OS/2, VMS, Amiga, BeOS, QNX, DJGPP/GO32, and ancient compiler branches have been retired.
- **Architecture & Extensibility**: The project separates the editor core from runtime-loaded UI drivers. The default curses UI and the no-curses LLM UI both implement the neutral driver surface. For a detailed breakdown, see the [Architecture Overview](doc/architecture.md) and [Cursor Driver Architecture](doc/cursor-driver-architecture.md).
- **LLM Driver**: `the --driver llm` exposes real THE runtime state through semantic snapshots and normalized input events without screen scraping curses output. For the agent-facing contract, see the [LLM Mode Agent Guide](doc/llm-mode.md), [LLM Driver Agent Guide](doc/llm-driver-agent-guide.md), and [LLM Driver Capability Inventory](doc/llm-driver-capabilities.md).

## 2. CREXX Integration
This fork is explicitly tailored for the modern [CREXX](https://github.com/crexx-org) scripting engine.

### Current Integration:
1. **Hosted Profiles and Macros**: THE builds with `NOREXX=1` for legacy interpreters, but `USE_CREXX=ON` enables the `src/crexx.c` bridge through `crexxsaa`.
2. **ADDRESS THE**: CREXX scripts run with a native `ADDRESS THE` environment, so profile and macro commands flow through THE's existing command subsystem.
3. **Source Cache**: CREXX source profiles/macros are compiled and cached by `crexxsaa`; existing `.rxbin` macros can be run directly.
4. **Documentation**: The current bridge contract is documented in [CREXX Integration](doc/crexx.md).

### Remaining Goals:
1. **Platform Testing**: macOS and local development workflows are the main proven path. Windows and Linux should continue to be validated in CI and release smoke tests, with special attention to runtime driver module loading.
2. **Packaging**: The CMake `install()` targets stage the executable and resources into a release directory. Future updates may involve integrating `CPack` to generate distributable `.dmg`, `.deb`, or `.zip` files.
