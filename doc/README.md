# CRXTHE documentation

These guides describe the implementation in this source tree. Check an
installed package's version and manifest before applying them to an older
release. CRXTHE is an independent fork based on Mark Hessling's THE 4.0.

## Using the editor

- [First launch and editing keys](first-launch.md)
- [Windows installation and first use](Windows-Readme.txt)
- [Command reference](commands/index.md) and [installed help](THE_Help.txt)
- [Macro interpreters, runtime selection and cache](rexx-interpreters.md)
- [Macro panels](panels.md)
- [LLM protocol and agent workflows](llm-driver-agent-guide.md)
- [LLM capability inventory](llm-driver-capabilities.md)
- [RXBIN tools, profiles and console](runtime-tools.md)
- [Native browser frontend](web-driver-poc.md)
- [WebAssembly browser preview and persistent workspace](browser-wasm.md)
- [Source highlighting for HTML and TeX](syntax-highlighting.md)
- [Batch Markdown example format and runner](batch-md-rexx-format.md)
- [Private prefix-macro package boundary](private-compatibility.md)

## Developing and packaging

- [Architecture and module ownership](architecture.md)
- [Cursor and driver contracts](cursor-driver-architecture.md)
- [Driver operations](driver-vtable-review.md)
- [Unicode text, layout and terminal profiles](utf-design.md)
- [cREXX hosting](crexx.md)
- [Parser integration](sdslh.md)
- [LLM implementation and protocol model](llm-mode.md)
- [Batch renderer architecture](batch-md-rexx-design.md)
- [Build and test guide](testing.md), including [browser acceptance](web-driver-test-plan.md)
- [Packaging](packaging.md) and [release signing](release-signing.md)

## Known issues

[Known issues and limitations](known-issues.md) records current defects,
workarounds and unsupported behavior. Feature descriptions must retain those
limits; an old test receipt is not evidence that a defect has been fixed.

## Roadmap

[The roadmap](roadmap.md) separates planned work and outstanding qualification
from implemented features. It contains acceptance conditions rather than a
progress diary. Git history and CI retain change history and execution logs.
The [WebAssembly browser implementation plan](wasm-browser-plan.md) describes
the implemented local browser runtime and its remaining acceptance criteria.

Documentation belongs with its user task or owning component. Keep a single
description of each contract, link to it from related guides, and update it
when behavior changes. Do not add session handovers, personal paths, dated
test totals or implementation journals to the product documentation.
