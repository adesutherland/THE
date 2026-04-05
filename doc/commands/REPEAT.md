# REPEAT
**repeat the last command**

## Syntax
```text
REPEat [target]
```

## Description
The REPEAT command advances the current line and executes the last command. It is equivalent to
NEXT 1 (or UP 1) and = for the specified number of times specified by target .
To determine how many lines on which to execute the last command, THE uses the target to
determine how many lines from the current position to the target. This is the number of times the last
command is executed.
If the last command to be executed, changes the current line, (because it has a target specification),
the next execution of the last command will begin from where the previous execution of last
command ended.

## Compatibility
XEDIT: Compatible.
KEDIT: Compatible.

## Status
Complete
