/* Classic Rexx: Unicode text, loops and a local routine. */
/* Run this source directly; no build is needed. */
greeting = 'Hello, λ'
total = 0
do index = 1 to 3
  total = total + index
end index
if total = 6 then say greeting
else say 'Unexpected total:' total
/* A continuation and a nested /* comment */. */
say 'Total:',
    total
call showtotal total
exit 0

showtotal: procedure
  parse arg amount
  say 'Routine received:' amount
  return
