# PDCursesMod Vendor Subset

This tree intentionally keeps only the pieces needed by THE's supported
Windows console build:

- the shared PDCurses public headers
- the core `pdcurses` sources
- the `wincon` backend used by `wincon_pdcurses`
- small common helpers included by the wincon backend
- the CMake helpers required to build that target

Other PDCurses backends, demos, bundled CI, old makefiles, and package recipes
were removed because current THE CMake does not build or test them.
