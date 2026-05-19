cmake_minimum_required(VERSION 3.10)

project(THE C)

set(THE_VERSION "4.0.CREXX")
set(THE_VERSION_DATE "02-Apr-2026")

# --- Options ---
option(USE_XCURSES "Build with XCurses (X11)" OFF)
option(USE_SDLCURSES "Build with SDL Curses" OFF)
option(USE_VTCURSES "Build with VT Curses" OFF)
option(USE_WIDE_CHAR "Enable wide character support" ON)
option(USE_UTF8 "Enable UTF-8 support" ON)
option(USE_UTF8PROC "Use utf8proc for Unicode grapheme clusters and character widths" ON)
option(USE_SDSLH "Build with DSL Syntax Highlighter support" ON)
option(USE_CREXX "Build with embedded CREXX profile/macro support" ON)
set(DSLSH_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../DSL-Syntax-Highlighter"
        CACHE PATH "Path to a local DSL-Syntax-Highlighter checkout")
option(DSLSH_PREFER_LOCAL "Prefer a local/sibling DSL-Syntax-Highlighter checkout when present" ON)
set(DSLSH_GIT_REPOSITORY "https://github.com/adesutherland/DSL-Syntax-Highlighter.git"
        CACHE STRING "DSL-Syntax-Highlighter Git repository")
set(DSLSH_GIT_TAG "develop"
        CACHE STRING "DSL-Syntax-Highlighter Git branch, tag, or commit SHA")

if(USE_UTF8 AND NOT USE_WIDE_CHAR)
    message(STATUS "USE_UTF8 requires wide character support; enabling USE_WIDE_CHAR")
    set(USE_WIDE_CHAR ON CACHE BOOL "Enable wide character support" FORCE)
endif()

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
    HAVE_RESIZETERM=1
    CAN_RESIZE=1
    NOREXX=1
    HAVE_WATTRSET=1
    HAVE_TOUCHLINE=1
    THE_TRACE=1
)

if(NOT WIN32)
    add_compile_definitions(USE_NCURSES=1)
endif()

# Standard and Warnings
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Only add warning-silencing flags for compilers that support and use them.
# The flag below is a Clang-specific warning control; GCC (cc1) does not recognize it.
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-Wno-deprecated-non-prototype" HAS_FLAG_NO_DEPRECATED_NON_PROTOTYPE)
    if(HAS_FLAG_NO_DEPRECATED_NON_PROTOTYPE)
        add_compile_options(-Wno-deprecated-non-prototype)
    endif()
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
check_include_file("curses.h" HAVE_CURSES_H)
check_include_file("ncurses.h" HAVE_NCURSES_H)
check_include_file("ncursesw/ncurses.h" HAVE_NCURSESW_NCURSES_H)
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
set(HAVE_CURS_SET 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})


# --- Find Dependencies ---
if(WIN32)
    if(USE_WIDE_CHAR)
        set(PDC_WIDE ON CACHE BOOL "Build PDCurses with wide character support" FORCE)
        add_compile_definitions(PDC_WIDE)
    endif()
    if(USE_UTF8)
        set(PDC_UTF8 ON CACHE BOOL "Build PDCurses with forced UTF-8 support" FORCE)
        add_compile_definitions(PDC_FORCE_UTF8)
    endif()
    add_subdirectory(src/PDCursesMod EXCLUDE_FROM_ALL)
    set(CURSES_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/PDCursesMod)
    set(CURSES_LIBRARIES wincon_pdcurses)
else()
    if((USE_WIDE_CHAR OR USE_UTF8) AND NOT APPLE)
        set(CURSES_NEED_WIDE TRUE)
    endif()
    find_package(Curses REQUIRED)
endif()

if(USE_SDSLH)
    add_compile_definitions(USE_SDSLH=1)

    if(DSLSH_PREFER_LOCAL AND EXISTS "${DSLSH_LOCAL_DIR}/codebuffer/CMakeLists.txt")
        message(STATUS "Using local DSL-Syntax-Highlighter at ${DSLSH_LOCAL_DIR}")
        set(DSLSH_SOURCE_DIR "${DSLSH_LOCAL_DIR}")
        add_subdirectory("${DSLSH_SOURCE_DIR}/codebuffer" "${CMAKE_CURRENT_BINARY_DIR}/sdslh-codebuffer" EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        message(STATUS "Fetching DSL-Syntax-Highlighter ${DSLSH_GIT_TAG} from ${DSLSH_GIT_REPOSITORY}")
        FetchContent_Declare(
            dsl_syntax_highlighter
            GIT_REPOSITORY ${DSLSH_GIT_REPOSITORY}
            GIT_TAG ${DSLSH_GIT_TAG}
            SOURCE_SUBDIR codebuffer
        )
        FetchContent_MakeAvailable(dsl_syntax_highlighter)
        FetchContent_GetProperties(dsl_syntax_highlighter)
        set(DSLSH_SOURCE_DIR "${dsl_syntax_highlighter_SOURCE_DIR}")
    endif()

    if(TARGET dslsyntax_editor)
        target_include_directories(dslsyntax_editor PUBLIC "${DSLSH_SOURCE_DIR}/codebuffer")
    endif()
    if(NOT TARGET DSLSH::editor AND TARGET dslsyntax_editor)
        add_library(DSLSH::editor ALIAS dslsyntax_editor)
    endif()

    find_program(THE_MDP_EXECUTABLE
        NAMES mdp
        HINTS
            "${DSLSH_LOCAL_DIR}/cmake-build-debug/parsers/markdown"
            "${DSLSH_LOCAL_DIR}/cmake-build-release/parsers/markdown"
            "$ENV{HOME}/.local/bin"
    )
    if(THE_MDP_EXECUTABLE)
        message(STATUS "Found Markdown SDSLH parser: ${THE_MDP_EXECUTABLE}")
    endif()
endif()

if(USE_UTF8 AND USE_UTF8PROC)
    find_package(utf8proc QUIET)
    if(TARGET utf8proc::utf8proc)
        set(THE_UTF8PROC_TARGET utf8proc::utf8proc)
    elseif(TARGET utf8proc)
        set(THE_UTF8PROC_TARGET utf8proc)
    else()
        include(FetchContent)
        set(UTF8PROC_INSTALL OFF CACHE BOOL "Enable installation of utf8proc" FORCE)
        set(UTF8PROC_ENABLE_TESTING OFF CACHE BOOL "Enable testing of utf8proc" FORCE)
        FetchContent_Declare(
            utf8proc
            GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
            GIT_TAG v2.11.3
        )
        FetchContent_MakeAvailable(utf8proc)
        set(THE_UTF8PROC_TARGET utf8proc)
    endif()
    add_compile_definitions(USE_UTF8PROC=1)
endif()

set(THE_USE_CREXX FALSE)
if(USE_CREXX)
    find_library(CREXXSAA_LIBRARY
        NAMES crexxsaa
        HINTS
            "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-debug/bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-release/bin"
            "$ENV{HOME}/.local/bin"
    )
    find_path(CREXXSAA_INCLUDE_DIR
        NAMES crexxsaa.h
        HINTS
            "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/interpreter"
            "$ENV{HOME}/.local/include"
            "$ENV{HOME}/.local/bin"
    )
    find_program(CREXX_RXC_EXECUTABLE
        NAMES rxc
        HINTS
            "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-debug/bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-release/bin"
            "$ENV{HOME}/.local/bin"
    )
    find_program(CREXX_RXAS_EXECUTABLE
        NAMES rxas
        HINTS
            "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-debug/bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-release/bin"
            "$ENV{HOME}/.local/bin"
    )
    find_file(CREXX_LIBRARY_RXBIN
        NAMES library.rxbin
        HINTS
            "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-debug/bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-release/bin"
            "$ENV{HOME}/.local/bin"
    )
    if(CREXXSAA_LIBRARY AND CREXXSAA_INCLUDE_DIR AND CREXX_RXC_EXECUTABLE AND CREXX_RXAS_EXECUTABLE AND CREXX_LIBRARY_RXBIN)
        get_filename_component(CREXX_IMPORT_DIR "${CREXX_LIBRARY_RXBIN}" DIRECTORY)
        get_filename_component(CREXXSAA_LIBRARY_DIR "${CREXXSAA_LIBRARY}" DIRECTORY)
        set(THE_USE_CREXX TRUE)
        message(STATUS "Found CREXXSAA library: ${CREXXSAA_LIBRARY}")
        message(STATUS "Found CREXX import directory: ${CREXX_IMPORT_DIR}")
        add_compile_definitions(
            USE_CREXX=1
            THE_CREXX_RXC="${CREXX_RXC_EXECUTABLE}"
            THE_CREXX_RXAS="${CREXX_RXAS_EXECUTABLE}"
            THE_CREXX_IMPORT_DIR="${CREXX_IMPORT_DIR}"
            THE_CREXX_LIBRARY_RXBIN="${CREXX_LIBRARY_RXBIN}"
        )
    else()
        message(STATUS "CREXXSAA support disabled; missing crexxsaa, rxc, rxas, or library.rxbin")
    endif()
endif()

# --- Include Directories ---
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CURSES_INCLUDE_DIRS}
)

if(THE_USE_CREXX)
    include_directories(${CREXXSAA_INCLUDE_DIR})
endif()

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
    src/crexx.c
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
    src/textedit.c
    src/textpos.c
    src/utf8term.c
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
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(${EXECUTABLE_NAME} ${THE_UTF8PROC_TARGET})
endif()

if(USE_SDSLH)
    target_link_libraries(${EXECUTABLE_NAME} DSLSH::editor)
endif()
if(THE_USE_CREXX)
    target_link_libraries(${EXECUTABLE_NAME} ${CREXXSAA_LIBRARY})
    if(APPLE)
        set(THE_CREXX_INSTALL_RPATH "@loader_path")
    else()
        set(THE_CREXX_INSTALL_RPATH "$ORIGIN")
    endif()
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        BUILD_RPATH "${CREXXSAA_LIBRARY_DIR}"
        INSTALL_RPATH "${THE_CREXX_INSTALL_RPATH}"
    )
endif()

# Setup Release Directory in Build Folder
set(RELEASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/release")

add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${EXECUTABLE_NAME}> ${RELEASE_DIR}/${EXECUTABLE_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/doc/THE_Help.txt ${RELEASE_DIR}/THE_Help.txt
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/profile.the ${RELEASE_DIR}/profile.the
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/profile_crexx.the ${RELEASE_DIR}/profile_crexx.the
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/crexxcompile.the ${RELEASE_DIR}/crexxcompile.the
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/crexxrun.the ${RELEASE_DIR}/crexxrun.the
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/syntax ${RELEASE_DIR}/syntax
    COMMENT "Copying executable and resources to ${RELEASE_DIR}"
)

if(WIN32)
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${CURSES_LIBRARIES}> ${RELEASE_DIR}/
        COMMENT "Copying PDCurses DLL to ${RELEASE_DIR}"
    )
endif()

if(THE_MDP_EXECUTABLE)
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${THE_MDP_EXECUTABLE} ${RELEASE_DIR}/mdp
        COMMENT "Copying Markdown SDSLH parser to ${RELEASE_DIR}"
    )
endif()

if(USE_UTF8 AND USE_WIDE_CHAR)
    add_executable(utf8_terminal_probe tools/utf8_terminal_probe.c)
    target_include_directories(utf8_terminal_probe PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(utf8_terminal_probe ${CURSES_LIBRARIES})
endif()

# Install Targets
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  # Fallback to local user bin to avoid sudo permission errors during testing
  if(WIN32)
    set(CMAKE_INSTALL_PREFIX "$ENV{USERPROFILE}/.local" CACHE PATH "Default install prefix" FORCE)
  else()
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Default install prefix" FORCE)
  endif()
endif()

install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)
if(THE_MDP_EXECUTABLE)
    install(PROGRAMS ${THE_MDP_EXECUTABLE} DESTINATION bin)
endif()
if(WIN32)
    install(FILES $<TARGET_FILE:${CURSES_LIBRARIES}> DESTINATION bin)
endif()
install(FILES doc/THE_Help.txt profile.the profile_crexx.the crexxcompile.the crexxrun.the DESTINATION share/the)
install(DIRECTORY syntax/ DESTINATION share/the/syntax)

# --- Automated Testing ---
enable_testing()
add_executable(test_textpos tests/test_textpos.c src/textpos.c)
target_include_directories(test_textpos PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_textpos ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_textpos PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_textpos COMMAND test_textpos)
if(USE_UTF8)
    add_executable(test_utf8_fixture tests/test_utf8_fixture.c src/textpos.c)
    target_include_directories(test_utf8_fixture PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utf8_fixture ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utf8_fixture PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utf8_fixture COMMAND test_utf8_fixture ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/utf8-render.txt)

    add_executable(test_textedit tests/test_textedit.c src/textedit.c src/textpos.c src/utf8term.c)
    target_include_directories(test_textedit PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_textedit ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_textedit PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_textedit COMMAND test_textedit)

    add_executable(test_utf8term tests/test_utf8term.c src/utf8term.c src/textpos.c)
    target_include_directories(test_utf8term PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utf8term ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utf8term PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utf8term
        COMMAND test_utf8term
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/utf8_terminal_profiles/defaults-poc34.the
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/utf8_terminal_profiles/macos-apple-terminal-poc34-overrides.the)
endif()
add_test(NAME test_sdslh_integration COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sdslh_integration.sh)
set_tests_properties(test_sdslh_integration PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME test_sdslh_multifile_lifecycle COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sdslh_multifile_lifecycle.sh)
set_tests_properties(test_sdslh_multifile_lifecycle PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME test_file_ecolor_inheritance COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_file_ecolor_inheritance.sh)
set_tests_properties(test_file_ecolor_inheritance PROPERTIES SKIP_RETURN_CODE 77)
if(THE_USE_CREXX)
    add_test(NAME test_crexx_profile COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_crexx_profile.sh)
    set_tests_properties(test_crexx_profile PROPERTIES SKIP_RETURN_CODE 77)
endif()
