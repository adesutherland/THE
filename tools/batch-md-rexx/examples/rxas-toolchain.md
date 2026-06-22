# CREXX RXAS Toolchain Smoke

This example validates a small CREXX toolchain loop from inside a runnable REXX
example. The source block is highlighted as REXX. Its output is declared as
`rxas`, so the generated disassembly is highlighted by the CREXX RXAS parser.

## Disassembly

```rexx id=rxas-toolchain run=true kind=standalone output=rxas
options levelb
import rxfnsb

source = .string[]
call arrayappend source, "options levelb"
call arrayappend source, "say ""compiled through RXAS"""
call arrayappend source, "exit 0"

mkout = .string[]
mkerr = .string[]
address command "mktemp -d /tmp/the-rxas-toolchain.XXXXXX" output mkout error mkerr
if rc <> 0 then exit rc
work_dir = mkout[1]

source_path = work_dir || "/demo.crexx"
rxas_path = work_dir || "/demo.rxas"
rxbin_path = work_dir || "/demo.rxbin"
dis_stem = work_dir || "/demo.dis"
dis_path = dis_stem || ".rxas"

if write_lines(source_path, source) <> 0 then do
  call cleanup work_dir
  exit 2
end

command_lines = .string[]
command_lines[1] = "set -e"
command_lines[2] = "rxc -o " || shell_quote(rxas_path) || " " || shell_quote(source_path)
command_lines[3] = "rxas -o " || shell_quote(rxbin_path) || " " || shell_quote(rxas_path)
command_lines[4] = "rxdas -o " || shell_quote(dis_stem) || " " || shell_quote(rxbin_path)

tool_out = .string[]
tool_err = .string[]
address command "sh" input command_lines output tool_out error tool_err
tool_rc = rc
if tool_rc <> 0 then do
  call cleanup work_dir
  exit tool_rc
end

do forever
  available = lines(dis_path)
  if available < 0 then do
    call cleanup work_dir
    exit 2
  end
  if available = 0 then leave
  say linein(dis_path)
end

call cleanup work_dir
exit 0

write_lines: procedure = .int
  arg path = .string, values = .string[]
  do wi = 1 to values.0
    write_rc = lineout(path, values[wi])
    if write_rc <> 0 then return 1
  end
  call lineout(path)
return 0

cleanup: procedure
  arg path = .string
  if path = "" then return
  address command "rm -rf " || shell_quote(path)
return

shell_quote: procedure = .string
  arg value = .string
  quoted = "'"
  do qi = 1 to length(value)
    ch = substr(value, qi, 1)
    if ch = "'" then quoted = quoted || "'""'""'"
    else quoted = quoted || ch
  end
return quoted || "'"
```

## Program stdout

The same tiny program writes one line when run.

```rexx id=program-stdout run=true kind=standalone output=text
options levelb
say "compiled through RXAS"
```
