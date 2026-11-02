/* Classic Rexx: inspect a sentence with the standard word functions. */
sentence = 'The browser editor runs Classic Rexx'
say 'Sentence:' sentence
say 'Words:' words(sentence)
do position = 1 to words(sentence)
  say right(position, 2)':' word(sentence, position)
end
say 'Reversed:' reverse(sentence)
exit 0
