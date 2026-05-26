cmake_minimum_required(VERSION 3.11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "MinSizeRel" CACHE STRING "Choose the type of build, options are: Debug, Release, or MinSizeRel." FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to MinSizeRel.")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include (get_version)
project(pdcurses VERSION "${CURSES_VERSION}" LANGUAGES C)

if(MSVC)
    set(CMAKE_DEBUG_POSTFIX d)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")  # enable parallel builds
endif()

message(STATUS "Generator .............. ${CMAKE_GENERATOR}")
message(STATUS "Build Type ............. ${CMAKE_BUILD_TYPE}")

include(build_options)

message(STATUS "PDC Version ............ ${PROJECT_VERSION}")

include(gen_config_header)

file(GLOB pdcurses_src_files pdcurses/*.c)

if(WIN32)
    add_subdirectory(wincon)
else()
    message(STATUS "Bundled PDCursesMod is retained only for the Windows wincon build.")
endif()
