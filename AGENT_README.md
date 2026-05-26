# THE (The Hessling Editor) - Agent Project Overview

## 1. Current Status (v4.0.CREXX Fork)
**THE** is a full-screen, character-mode text editor based on the VM/CMS editor XEDIT and KEDIT, originally written by Mark Hessling. This repository represents a significantly modernized fork.

### Recent Modernizations:
- **Build System**: The legacy GNU Autotools (`configure`) and various scattered `Makefile` scripts for DOS/OS2/Windows have been completely replaced with a unified, cross-platform **CMake** build system (`CMakeLists.txt`).
- **IDE Support**: The project is now natively supported by modern C/C++ IDEs like CLion out-of-the-box.
- **Codebase Standardization**: The entire C codebase has been strictly refactored from pre-ANSI (K&R) C to the **C99 standard**. All legacy `#ifdef HAVE_PROTO` blocks and `Args()` macros were stripped to ensure strict compliance and eliminate hundreds of compiler warnings on modern Clang/GCC compilers.
- **Architecture & Extensibility**: The project maintains a clean separation between file data models, ncurses rendering views, and command execution logic. For a detailed breakdown of the codebase structure, see the [Architecture Overview](doc/architecture.md).
- **LLM Driver Direction**: The emerging LLM mode exposes a logical screen/cursor view and normalized input events without screen scraping curses output. For the agent-facing contract and current limitations, see the [LLM Mode Agent Guide](doc/llm-mode.md).

## 2. CREXX Integration
This fork is explicitly tailored for the modern [CREXX](https://github.com/crexx-org) scripting engine.

### Current Integration:
1. **Hosted Profiles and Macros**: THE builds with `NOREXX=1` for legacy interpreters, but `USE_CREXX=ON` enables the `src/crexx.c` bridge through `crexxsaa`.
2. **ADDRESS THE**: CREXX scripts run with a native `ADDRESS THE` environment, so profile and macro commands flow through THE's existing command subsystem.
3. **Source Cache**: CREXX source profiles/macros are compiled and cached by `crexxsaa`; existing `.rxbin` macros can be run directly.
4. **Documentation**: The current bridge contract is documented in [CREXX Integration](doc/crexx.md).

### Remaining Goals:
1. **Platform Testing**: macOS and local development workflows are the main proven path. Windows and Linux should continue to be validated in CI and release smoke tests.
2. **Packaging**: The CMake `install()` targets stage the executable and resources into a release directory. Future updates may involve integrating `CPack` to generate distributable `.dmg`, `.deb`, or `.zip` files.
