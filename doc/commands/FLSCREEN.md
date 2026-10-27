# FLSCREEN

```text
QUERY FLSCREEN
EXTRACT /FLSCREEN/
```

FLSCREEN reports the file range currently visible in the active logical view.
FLSCREEN.0 is 2, FLSCREEN.1 is the first displayed file line, and FLSCREEN.2 is
the last. TOF/EOF, reserved and shadow summary rows are excluded. If no real file
rows are visible the range is 0 0. In batch mode FLSCREEN.0 is 0.

The range is computed from a fresh shared screen layout, including changes
made during macros, scrolling, splits and frontend resizing. The same query
works in curses, LLM and browser frontends; it does not change OS window size or
enter a graphical fullscreen mode. There is no SET FLSCREEN option.

Reference: [IBM EXTRACT](https://www.ibm.com/docs/en/zvm/7.4.0?topic=macros-extract).
