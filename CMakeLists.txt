cmake_minimum_required(VERSION 3.10)

project(THE C)

set(THE_VERSION "4.0.CREXX")
set(THE_VERSION_DATE "02-Apr-2026")

# --- Options ---
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

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  # Fallback to local user bin to avoid sudo permission errors during testing
  if(WIN32)
    set(CMAKE_INSTALL_PREFIX "$ENV{USERPROFILE}/.local" CACHE PATH "Default install prefix" FORCE)
  else()
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Default install prefix" FORCE)
  endif()
endif()

if(USE_UTF8 AND NOT USE_WIDE_CHAR)
    message(STATUS "USE_UTF8 requires wide character support; enabling USE_WIDE_CHAR")
    set(USE_WIDE_CHAR ON CACHE BOOL "Enable wide character support" FORCE)
endif()

if(APPLE)
    set(THE_PLATFORM_NAME "osx")
elseif(WIN32)
    set(THE_PLATFORM_NAME "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(THE_PLATFORM_NAME "linux")
elseif(UNIX)
    set(THE_PLATFORM_NAME "unix")
else()
    set(THE_PLATFORM_NAME "generic")
endif()
set(THE_SYSTEM_PROFILE_NAME "system-${THE_PLATFORM_NAME}.the")
set(THE_SYSTEM_PROFILE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/the")
set(THE_SYSTEM_PROFILE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${THE_SYSTEM_PROFILE_NAME}")

# Determine the supported curses variant.
set(THE_VARIANT "con")
if(USE_WIDE_CHAR)
    set(THE_VARIANT "conw")
    add_compile_definitions(USE_WIDE_CHAR)
endif()
if(USE_UTF8)
    set(THE_VARIANT "conw8")
    add_compile_definitions(USE_UTF8 USE_WIDE_CHAR)
endif()

# --- Defines ---
add_compile_definitions(
    HAVE_CONFIG_H
    THE_VERSION="${THE_VERSION}"
    THE_VERSION_DATE="${THE_VERSION_DATE}"
    THE_VARIANT="${THE_VARIANT}"
    THE_PLATFORM_NAME="${THE_PLATFORM_NAME}"
    THE_SYSTEM_PROFILE_NAME="${THE_SYSTEM_PROFILE_NAME}"
    THE_SYSTEM_PROFILE_DIR="${THE_SYSTEM_PROFILE_INSTALL_DIR}"
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
            GIT_SHALLOW TRUE
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
    src/cursesdriver.c
    src/cursor.c
    src/default.c
    src/directry.c
    src/edit.c
    src/error.c
    src/execute.c
    src/file.c
    src/thematch.c
    src/getch.c
    src/inputevent.c
    src/linked.c
    src/logcursor.c
    src/llmdriver.c
    src/llmruntime.c
    src/mouse.c
    src/mousehit.c
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
    src/screenframe.c
    src/show.c
    src/single.c
    src/sort.c
    src/target.c
    src/the.c
    src/textedit.c
    src/textpos.c
    src/transientui.c
    src/uidriver.c
    src/utflayout.c
    src/utfrepair.c
    src/utfterm.c
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

if(USE_UTF8 AND EXISTS "${THE_SYSTEM_PROFILE_SOURCE}")
    add_custom_command(
        OUTPUT ${RELEASE_DIR}/${THE_SYSTEM_PROFILE_NAME}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${THE_SYSTEM_PROFILE_SOURCE} ${RELEASE_DIR}/${THE_SYSTEM_PROFILE_NAME}
        DEPENDS ${THE_SYSTEM_PROFILE_SOURCE}
        COMMENT "Copying UTF system profile ${THE_SYSTEM_PROFILE_NAME} to ${RELEASE_DIR}"
    )
    add_custom_target(copy_utf_system_profile ALL
        DEPENDS ${RELEASE_DIR}/${THE_SYSTEM_PROFILE_NAME}
    )
endif()

if(WIN32)
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${CURSES_LIBRARIES}> ${RELEASE_DIR}/
        COMMENT "Copying PDCurses DLL to ${RELEASE_DIR}"
    )
endif()

if(USE_UTF8 AND USE_WIDE_CHAR)
    add_executable(utf_terminal_probe tools/utf_terminal_probe.c)
    target_include_directories(utf_terminal_probe PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(utf_terminal_probe ${CURSES_LIBRARIES})
endif()

add_executable(the_agent
    tools/the_agent.c
    src/agentdriver.c
    src/llmdriver.c
    src/inputevent.c
    src/uidriver.c
    src/logcursor.c
    src/textpos.c
)
target_include_directories(the_agent PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(the_agent ${THE_UTF8PROC_TARGET})
    target_compile_definitions(the_agent PRIVATE USE_UTF8PROC=1)
endif()

add_executable(the_llm_headless
    tools/the_llm_headless.c
    src/agentdriver.c
    src/llmdriver.c
    src/inputevent.c
    src/uidriver.c
    src/logcursor.c
    src/textpos.c
    src/transientui.c
)
target_include_directories(the_llm_headless PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(the_llm_headless ${THE_UTF8PROC_TARGET})
    target_compile_definitions(the_llm_headless PRIVATE USE_UTF8PROC=1)
endif()

# Install Targets
install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)
if(WIN32)
    install(FILES $<TARGET_FILE:${CURSES_LIBRARIES}> DESTINATION bin)
endif()
install(FILES doc/THE_Help.txt profile.the profile_crexx.the crexxcompile.the crexxrun.the DESTINATION share/the)
if(USE_UTF8)
    install(FILES "${RELEASE_DIR}/${THE_SYSTEM_PROFILE_NAME}" DESTINATION share/the OPTIONAL)
endif()
install(DIRECTORY syntax/ DESTINATION share/the/syntax)

# --- Automated Testing ---
enable_testing()
add_test(NAME test_curses_boundary
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curses_boundary.sh
            ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME test_curses_boundary_inventory
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/inventory_direct_curses.sh
            ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_textpos tests/test_textpos.c src/textpos.c)
target_include_directories(test_textpos PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_textpos ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_textpos PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_textpos COMMAND test_textpos)
add_executable(test_logcursor tests/test_logcursor.c src/logcursor.c src/textpos.c)
target_include_directories(test_logcursor PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_logcursor ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_logcursor PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_logcursor COMMAND test_logcursor)
add_executable(test_inputevent tests/test_inputevent.c src/inputevent.c)
target_include_directories(test_inputevent PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME test_inputevent COMMAND test_inputevent)
add_executable(test_transientui tests/test_transientui.c src/transientui.c)
target_include_directories(test_transientui PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME test_transientui COMMAND test_transientui)
add_executable(test_mousehit tests/test_mousehit.c src/mousehit.c src/inputevent.c)
target_include_directories(test_mousehit PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME test_mousehit COMMAND test_mousehit)
add_executable(test_uidriver tests/test_uidriver.c src/uidriver.c src/logcursor.c src/textpos.c)
target_include_directories(test_uidriver PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_uidriver ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_uidriver PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_uidriver COMMAND test_uidriver)
add_executable(test_screenframe tests/test_screenframe.c src/screenframe.c src/uidriver.c src/logcursor.c src/textpos.c)
target_include_directories(test_screenframe PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_screenframe ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_screenframe PRIVATE USE_UTF8PROC=1)
endif()
if(USE_SDSLH)
    target_link_libraries(test_screenframe DSLSH::editor)
endif()
add_test(NAME test_screenframe COMMAND test_screenframe)
add_executable(test_llmdriver tests/test_llmdriver.c src/llmdriver.c src/inputevent.c src/uidriver.c src/logcursor.c src/textpos.c)
target_include_directories(test_llmdriver PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_llmdriver ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_llmdriver PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_llmdriver COMMAND test_llmdriver)
add_executable(test_virtual_screen tests/test_virtual_screen.c src/llmdriver.c src/inputevent.c src/uidriver.c src/logcursor.c src/textpos.c)
target_include_directories(test_virtual_screen PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_virtual_screen ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_virtual_screen PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_virtual_screen COMMAND test_virtual_screen)
add_executable(test_agentdriver tests/test_agentdriver.c src/agentdriver.c src/llmdriver.c src/inputevent.c src/uidriver.c src/logcursor.c src/textpos.c)
target_include_directories(test_agentdriver PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_agentdriver ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_agentdriver PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_agentdriver COMMAND test_agentdriver)
add_test(NAME test_the_agent_script
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_the_agent_script.sh
            $<TARGET_FILE:the_agent>)
add_test(NAME test_the_agent_capabilities
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_the_agent_capabilities.sh
            $<TARGET_FILE:the_agent>)
add_test(NAME test_the_agent_no_curses
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_agent_no_curses.sh
            $<TARGET_FILE:the_agent>)
add_test(NAME test_the_llm_headless_no_curses
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_no_curses_symbols.sh
            $<TARGET_FILE:the_llm_headless>)
add_executable(test_llmruntime tests/test_llmruntime.c src/llmruntime.c src/llmdriver.c src/inputevent.c src/screenframe.c src/uidriver.c src/logcursor.c src/textpos.c)
target_include_directories(test_llmruntime PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_llmruntime ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_llmruntime PRIVATE USE_UTF8PROC=1)
endif()
if(USE_SDSLH)
    target_link_libraries(test_llmruntime DSLSH::editor)
endif()
add_test(NAME test_llmruntime COMMAND test_llmruntime)
if(USE_UTF8)
    add_executable(test_utf_fixture tests/test_utf_fixture.c src/textpos.c)
    target_include_directories(test_utf_fixture PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utf_fixture ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utf_fixture PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utf_fixture COMMAND test_utf_fixture ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/utf-render.txt)

    add_executable(test_textedit tests/test_textedit.c src/textedit.c src/textpos.c src/utfterm.c)
    target_include_directories(test_textedit PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_textedit ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_textedit PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_textedit COMMAND test_textedit)

    add_executable(test_utfterm tests/test_utfterm.c src/utfterm.c src/textpos.c)
    target_include_directories(test_utfterm PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utfterm ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utfterm PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utfterm
        COMMAND test_utfterm
            ${CMAKE_CURRENT_SOURCE_DIR}/system-osx.the)

    add_executable(test_utflayout tests/test_utflayout.c src/utflayout.c src/utfterm.c src/textpos.c)
    target_include_directories(test_utflayout PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utflayout ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utflayout PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utflayout COMMAND test_utflayout)

    add_executable(test_utfrepair tests/test_utfrepair.c src/utfrepair.c src/utfterm.c src/textpos.c)
    target_include_directories(test_utfrepair PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utfrepair ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utfrepair PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utfrepair COMMAND test_utfrepair)

    if(USE_WIDE_CHAR)
        add_test(NAME test_utf_terminal_probe_no_visual
            COMMAND utf_terminal_probe calibrate keycap --no-visual
                --profile-dir ${CMAKE_CURRENT_BINARY_DIR})
        add_test(NAME test_utf_terminal_probe_profile
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_utf_probe_profile.sh)
        set_tests_properties(test_utf_terminal_probe_profile PROPERTIES
            SKIP_RETURN_CODE 77
            ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
        )
    endif()
    add_test(NAME test_system_profile COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_system_profile.sh)
    set_tests_properties(test_system_profile PROPERTIES
        SKIP_RETURN_CODE 77
        ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif()
add_test(NAME test_sdslh_integration COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sdslh_integration.sh)
set_tests_properties(test_sdslh_integration PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
)
add_test(NAME test_sdslh_multifile_lifecycle COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sdslh_multifile_lifecycle.sh)
set_tests_properties(test_sdslh_multifile_lifecycle PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
)
add_test(NAME test_file_ecolor_inheritance COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_file_ecolor_inheritance.sh)
set_tests_properties(test_file_ecolor_inheritance PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
)
add_test(NAME test_message_history COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_message_history.sh)
set_tests_properties(test_message_history PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
)
if(THE_USE_CREXX)
    add_test(NAME test_field_queries COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_field_queries.sh)
    set_tests_properties(test_field_queries PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 10
        RESOURCE_LOCK the_pty_editor
        ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
    if(USE_UTF8)
        add_test(NAME test_field_queries_utf COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_field_queries.sh --utf)
        set_tests_properties(test_field_queries_utf PROPERTIES
            SKIP_RETURN_CODE 77
            TIMEOUT 10
            RESOURCE_LOCK the_pty_editor
            DEPENDS test_field_queries
            ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
        )
    endif()
    add_test(NAME test_normal_area_queries COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_normal_area_queries.sh)
    set_tests_properties(test_normal_area_queries PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 10
        RESOURCE_LOCK the_pty_editor
        ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
    add_test(NAME test_sos_navigation_queries COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sos_navigation_queries.sh)
    set_tests_properties(test_sos_navigation_queries PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 10
        RESOURCE_LOCK the_pty_editor
        DEPENDS test_normal_area_queries
        ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
    add_test(NAME test_sos_logical_edit_queries COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sos_logical_edit_queries.sh)
    set_tests_properties(test_sos_logical_edit_queries PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 10
        RESOURCE_LOCK the_pty_editor
        DEPENDS test_sos_navigation_queries
        ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
    add_test(NAME test_selective_change_prompt COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_selective_change_prompt.sh)
    set_tests_properties(test_selective_change_prompt PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 15
        RESOURCE_LOCK the_pty_editor
        DEPENDS test_sos_logical_edit_queries
        ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
    add_test(NAME test_crexx_profile COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_crexx_profile.sh)
    set_tests_properties(test_crexx_profile PROPERTIES
        SKIP_RETURN_CODE 77
        ENVIRONMENT "THE_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif()
