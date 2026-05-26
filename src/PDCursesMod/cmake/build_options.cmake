
option(PDC_BUILD_SHARED "Build dynamic libs for pdcurses" ON)
option(PDC_UTF8 "Force to UTF8" OFF)
option(PDC_WIDE "Wide - pulls in sdl-ttf" OFF)
option(PDCDEBUG "Debug tracing" OFF)
option(PDC_CHTYPE_32 "CHTYPE_32" OFF)

message(STATUS "PDC_BUILD_SHARED ....... ${PDC_BUILD_SHARED}")
message(STATUS "PDC_UTF8 ............... ${PDC_UTF8}")
message(STATUS "PDC_WIDE ............... ${PDC_WIDE}")
message(STATUS "PDCDEBUG ............... ${PDCDEBUG}")
message(STATUS "PDC_CHTYPE_32 .......... ${PDC_CHTYPE_32}")

# normalize a windows path
file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" CMAKE_INSTALL_PREFIX)
