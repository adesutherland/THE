# License incompatibility notice

The desktop curses driver includes `src/drivers/curses/getch.c`. Its Patrick J.
Wolfe notice permits use only if no money is made from the software, while the
GNU GPL in `LICENSE` permits commercial distribution. These conditions appear
incompatible. No exception or additional permission resolving this conflict
has been established.

The browser Wasm build excludes the curses driver and `getch.c`.
