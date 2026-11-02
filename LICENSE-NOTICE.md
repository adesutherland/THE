# License incompatibility notice

The macOS and Linux curses driver uses `src/drivers/curses/getch.c`. Its Patrick
J. Wolfe notice permits use only if no money is made from the software, while
the GNU GPL in `LICENSE` permits commercial distribution. These conditions
appear incompatible. No exception or additional permission resolving this
conflict has been established. The standard Windows PDCurses build compiles out
the file's key-handling implementation and calls PDCurses `wgetch` directly.

The browser Wasm build excludes the curses driver and `getch.c`.
