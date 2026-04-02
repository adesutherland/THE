cmake_minimum_required(VERSION 3.10)

project(THE C)

set(THE_VERSION "4.0.CREXX")
set(THE_VERSION_DATE "02-Apr-2026")

# --- Options ---
option(USE_XCURSES "Build with XCurses (X11)" OFF)
option(USE_SDLCURSES "Build with SDL Curses" OFF)
option(USE_VTCURSES "Build with VT Curses" OFF)
option(USE_WIDE_CHAR "Enable wide character support" OFF)
option(USE_UTF8 "Enable UTF-8 support" OFF)

# Determine basic curses type based on options
if(USE_XCURSES)
    set(THE_VARIANT "x11")
    if(USE_WIDE_CHAR)
        set(THE_VARIANT "x11w")
        add_compile_definitions(USE_WIDE_CHAR)
    endif()
    if(USE_UTF8)
        set(THE_VARIANT "x11w8")
        add_compile_definitions(USE_UTF8 USE_WIDE_CHAR)
    endif()
elseif(USE_SDLCURSES)
    set(THE_VARIANT "sdl")
    if(USE_WIDE_CHAR)
         set(THE_VARIANT "sdlw")
         add_compile_definitions(USE_WIDE_CHAR)
    endif()
    if(USE_UTF8)
         set(THE_VARIANT "sdlw8")
         add_compile_definitions(USE_UTF8 USE_WIDE_CHAR)
    endif()
elseif(USE_VTCURSES)
     set(THE_VARIANT "vt")
     if(USE_WIDE_CHAR)
         set(THE_VARIANT "vtw")
         add_compile_definitions(USE_WIDE_CHAR)
     endif()
     if(USE_UTF8)


         set(THE_VARIANT "vtw8")
         add_compile_definitions(USE_UTF8 USE_WIDE_CHAR)
     endif()
else()
    set(THE_VARIANT "con")
    if(USE_WIDE_CHAR)
        set(THE_VARIANT "conw")
        add_compile_definitions(USE_WIDE_CHAR)
    endif()
    if(USE_UTF8)
        set(THE_VARIANT "conw8")
        add_compile_definitions(USE_UTF8 USE_WIDE_CHAR)
    endif()
endif()

# --- Defines ---
add_compile_definitions(
    HAVE_CONFIG_H
    THE_VERSION="${THE_VERSION}"
    THE_VERSION_DATE="${THE_VERSION_DATE}"
    THE_VARIANT="${THE_VARIANT}"
    BUILD3001=1
    USE_NCURSES=1
    HAVE_RESIZETERM=1
    CAN_RESIZE=1
    NOREXX=1
    HAVE_WATTRSET=1
    HAVE_TOUCHLINE=1
    THE_TRACE=1
)

# Standard and Warnings
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-deprecated-non-prototype)
endif()

# Platform specifics
if(APPLE)
    add_compile_options(-Wno-implicit-function-declaration)
endif()

# --- Config Header (config.h) ---
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckTypeSize)

check_include_file("dirent.h" HAVE_DIRENT_H)
check_include_file("fcntl.h" HAVE_FCNTL_H)
check_include_file("locale.h" HAVE_LOCALE_H)
check_include_file("memory.h" HAVE_MEMORY_H)
check_include_file("stat.h" HAVE_STAT_H)
check_include_file("stdarg.h" HAVE_STDARG_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("time.h" HAVE_TIME_H)
check_include_file("sys/acl.h" HAVE_SYS_ACL_H)
check_include_file("sys/mode.h" HAVE_SYS_MODE_H)
check_include_file("sys/file.h" HAVE_SYS_FILE_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/socket.h" HAVE_SYS_SOCKET_H)
check_include_file("sys/time.h" HAVE_SYS_TIME_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("sys/wait.h" HAVE_SYS_WAIT_H)
check_include_file("sys/select.h" HAVE_SYS_SELECT_H)
check_include_file("select.h" HAVE_SELECT_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("ctype.h" HAVE_CTYPE_H)
check_include_file("wctype.h" HAVE_WCTYPE_H)
check_include_file("errno.h" HAVE_ERRNO_H)
check_include_file("alloca.h" HAVE_ALLOCA_H)

check_function_exists(fork HAVE_FORK)
check_function_exists(realpath HAVE_REALPATH)
check_function_exists(setlocale HAVE_SETLOCALE)
check_function_exists(sigaction HAVE_SIGACTION)
check_function_exists(siginterrupt HAVE_SIGINTERRUPT)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(stricmp HAVE_STRICMP)
check_function_exists(strcmpi HAVE_STRCMPI)
check_function_exists(readlink HAVE_READLINK)
check_function_exists(select HAVE_SELECT)
check_function_exists(mkfifo HAVE_MKFIFO)
check_function_exists(rename HAVE_RENAME)
check_function_exists(lstat HAVE_LSTAT)
check_function_exists(chown HAVE_CHOWN)
check_function_exists(mkstemp HAVE_MKSTEMP)
check_function_exists(getacl HAVE_GETACL)
check_function_exists(vsscanf HAVE_VSSCANF)
check_function_exists(usleep HAVE_USLEEP)
check_function_exists(poll HAVE_POLL)
check_function_exists(snprintf HAVE_SNPRINTF)
check_function_exists(vsnprintf HAVE_VSNPRINTF)

# Hardcode some stuff for now that normally autoconf tests
set(STDC_HEADERS 1)
set(UNIX 1)
set(RETSIGTYPE "void")
set(HAVE_WATTRSET 1)
set(HAVE_TOUCHLINE 1)
set(HAVE_KEYPAD 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})


# --- Find Dependencies ---
find_package(Curses REQUIRED)

# --- Include Directories ---
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CURSES_INCLUDE_DIRS}
)

# --- Source Files ---
set(THE_SOURCES
    src/box.c
    src/colour.c
    src/column.c
    src/comm1.c
    src/comm2.c
    src/comm3.c
    src/comm4.c
    src/comm5.c
    src/commset1.c
    src/commset2.c
    src/commsos.c
    src/commutil.c
    src/cursor.c
    src/default.c
    src/directry.c
    src/edit.c
    src/error.c
    src/execute.c
    src/file.c
    src/thematch.c
    src/getch.c
    src/linked.c
    src/mouse.c
    src/memory.c
    src/nonansi.c
    src/parser.c
    src/prefix.c
    src/print.c
    src/query.c
    src/query1.c
    src/query2.c
    src/regex.c
    src/reserved.c
    src/rexx.c
    src/scroll.c
    src/show.c
    src/single.c
    src/sort.c
    src/target.c
    src/the.c
    src/mygetopt.c
    src/trace.c
    src/util.c
)

if(USE_UTF8)
    list(APPEND THE_SOURCES src/contrib/utf8.c)
endif()

# --- Executable ---
set(EXECUTABLE_NAME "the")
add_executable(${EXECUTABLE_NAME} ${THE_SOURCES})
target_link_libraries(${EXECUTABLE_NAME} ${CURSES_LIBRARIES})

# Setup Release Directory in Build Folder
set(RELEASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/release")

add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${EXECUTABLE_NAME}> ${RELEASE_DIR}/${EXECUTABLE_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/doc/THE_Help.txt ${RELEASE_DIR}/THE_Help.txt
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/profile.the ${RELEASE_DIR}/profile.the
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/syntax ${RELEASE_DIR}/syntax
    COMMENT "Copying executable and resources to ${RELEASE_DIR}"
)

# Install Targets
install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)
install(FILES doc/THE_Help.txt profile.the DESTINATION share/the)
install(DIRECTORY syntax/ DESTINATION share/the/syntax)
