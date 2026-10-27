/* Classic Rexx running inside this browser. */
say 'Hello from the browser Rexx interpreter!'
say 'What is your name?'
parse pull name
if name = '' then name = 'visitor'
say 'Hello,' name
say ''
numeric digits 20
total = 0
do number = 1 to 5
  squares.number = number * number
  total = total + squares.number
  say right(number, 2) 'squared =' right(squares.number, 3)
end
say 'Total:' total
interpret "say 'INTERPRET works here too.'"
say 'Reversed name:' reverse(name)
say 'A local routine says:' greeting(name)
exit 0

greeting: procedure
  parse arg who
  return 'Welcome, ' || who || '!'
