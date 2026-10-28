# RX

**run RXBIN programs and manage the console, profiles and language tools**

## Syntax

```text
RX RUN [tool-id|filename.rxbin] [arguments...]
RX CONSOLE [OFF]
RX INPUT text
RX EOF
RX STOP
RX CLEAR
RX STATUS
RX PROFILE
RX BUILD
RX ACTIVATE
RX REGISTER id role image extensions "label"
RX LANGUAGE id
```

`RX RUN` runs the current Classic Rexx buffer, including unsaved edits.
A filename runs compiled bytecode; a registered ID selects its tool. IDs take
precedence over filenames. Quote paths and arguments containing spaces.
Subcommand keywords are case-insensitive; filenames, IDs and data keep their case.

Profiles use **Edit → Build → Activate**. Terminal and graphical consoles use
the shared job state and separate program stdin from editor commands.
See [RXBIN tools, profiles and console](../runtime-tools.md) for capabilities,
persistence, language registration and build requirements.
