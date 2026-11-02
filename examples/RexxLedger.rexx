/* Classic Rexx: arguments and a stem-backed running balance. */
opening = arg(1)
if opening = '' then opening = 20
balance.0 = opening
change.1 = 15
change.2 = -7
change.3 = 12
say 'Opening balance:' balance.0
do entry = 1 to 3
  balance.entry = balance.0 + change.entry
  balance.0 = balance.entry
  say 'Entry' entry 'change' change.entry 'balance' balance.entry
end
exit 0
