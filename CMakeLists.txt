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
file(GLOB THE_SYSTEM_PROFILE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/system-*.the")

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

if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--stack,16777216")
endif()

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
    set(THE_CREXXSAA_MIN_ABI_VERSION 3)
    set(THE_CREXX_LOCAL_DEBUG_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-debug/bin")
    set(THE_CREXX_LOCAL_RELEASE_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../CREXX/cmake-build-release/bin")
    if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
        set(THE_CREXX_LOCAL_BIN_DIRS
            "${THE_CREXX_LOCAL_RELEASE_BIN}"
            "${THE_CREXX_LOCAL_DEBUG_BIN}"
        )
    else()
        set(THE_CREXX_LOCAL_BIN_DIRS
            "${THE_CREXX_LOCAL_DEBUG_BIN}"
            "${THE_CREXX_LOCAL_RELEASE_BIN}"
        )
    endif()
    find_library(CREXXSAA_LIBRARY
        NAMES crexxsaa
        HINTS
            ${THE_CREXX_LOCAL_BIN_DIRS}
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
            ${THE_CREXX_LOCAL_BIN_DIRS}
            "$ENV{HOME}/.local/bin"
    )
    find_program(CREXX_RXAS_EXECUTABLE
        NAMES rxas
        HINTS
            ${THE_CREXX_LOCAL_BIN_DIRS}
            "$ENV{HOME}/.local/bin"
    )
    find_program(CREXX_DRIVER_EXECUTABLE
        NAMES crexx
        HINTS
            ${THE_CREXX_LOCAL_BIN_DIRS}
            "$ENV{HOME}/.local/bin"
    )
    find_file(CREXX_LIBRARY_RXBIN
        NAMES library.rxbin
        HINTS
            ${THE_CREXX_LOCAL_BIN_DIRS}
            "$ENV{HOME}/.local/bin"
    )
    set(CREXXSAA_ABI_OK FALSE)
    if(CREXXSAA_INCLUDE_DIR)
        file(STRINGS "${CREXXSAA_INCLUDE_DIR}/crexxsaa.h" CREXXSAA_ABI_LINE
            REGEX "^[ \t]*#define[ \t]+CREXXSAA_ABI_VERSION[ \t]+[0-9]+")
        if(CREXXSAA_ABI_LINE)
            string(REGEX REPLACE ".*CREXXSAA_ABI_VERSION[ \t]+([0-9]+).*" "\\1"
                CREXXSAA_ABI_VERSION "${CREXXSAA_ABI_LINE}")
            if(NOT CREXXSAA_ABI_VERSION LESS THE_CREXXSAA_MIN_ABI_VERSION)
                set(CREXXSAA_ABI_OK TRUE)
            endif()
        endif()
    endif()
    if(CREXXSAA_LIBRARY AND CREXXSAA_INCLUDE_DIR AND CREXXSAA_ABI_OK AND CREXX_RXC_EXECUTABLE AND CREXX_RXAS_EXECUTABLE AND CREXX_LIBRARY_RXBIN)
        get_filename_component(CREXX_IMPORT_DIR "${CREXX_LIBRARY_RXBIN}" DIRECTORY)
        get_filename_component(CREXXSAA_LIBRARY_DIR "${CREXXSAA_LIBRARY}" DIRECTORY)
        set(THE_USE_CREXX TRUE)
        message(STATUS "Found CREXXSAA library: ${CREXXSAA_LIBRARY}")
        message(STATUS "Found CREXXSAA ABI: ${CREXXSAA_ABI_VERSION}")
        message(STATUS "Found CREXX import directory: ${CREXX_IMPORT_DIR}")
        add_compile_definitions(
            USE_CREXX=1
            THE_CREXX_RXC="${CREXX_RXC_EXECUTABLE}"
            THE_CREXX_RXAS="${CREXX_RXAS_EXECUTABLE}"
            THE_CREXX_IMPORT_DIR="${CREXX_IMPORT_DIR}"
            THE_CREXX_LIBRARY_RXBIN="${CREXX_LIBRARY_RXBIN}"
        )
    else()
        message(STATUS "CREXXSAA support disabled; missing crexxsaa ABI ${THE_CREXXSAA_MIN_ABI_VERSION}+, rxc, rxas, or library.rxbin")
    endif()
endif()

# --- Include Directories ---
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/llm
    ${CMAKE_SOURCE_DIR}/src/drivers/llm
)
install(DIRECTORY tools/batch-md-rexx/templates DESTINATION share/the/batch-md-rexx)
install(DIRECTORY tools/batch-md-rexx/examples DESTINATION share/the/batch-md-rexx)
if(USE_UTF8)
    include_directories(${CMAKE_SOURCE_DIR}/src/contrib)
endif()

if(THE_USE_CREXX)
    include_directories(${CREXXSAA_INCLUDE_DIR})
endif()

# --- Source Files ---
set(THE_SOURCES
    src/box.c
    src/colour.c
    src/thecolour.c
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
    src/driverlayout.c
    src/edit.c
    src/error.c
    src/execute.c
    src/file.c
    src/thematch.c
    src/inputevent.c
    src/linked.c
    src/logcursor.c
    src/llm/llmdriver.c
    src/llm/llmruntime.c
    src/llm/llmsession.c
    src/mouse.c
    src/mousehit.c
    src/memory.c
    src/nonansi.c
    src/parser.c
    src/parserdiagnostics.c
    src/prefix.c
    src/print.c
    src/query.c
    src/query1.c
    src/query2.c
    src/rendercell.c
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
    src/thedriver.c
    src/textedit.c
    src/textpos.c
    src/utfinput.c
    src/transientui.c
    src/uidriver.c
    src/utfcluster.c
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
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    THE_DRIVER_MODULE_SUFFIX="${CMAKE_SHARED_MODULE_SUFFIX}"
    THE_DRIVER_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}/lib/the/drivers"
)
set_target_properties(${EXECUTABLE_NAME} PROPERTIES ENABLE_EXPORTS ON)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${EXECUTABLE_NAME} PRIVATE "-Wl,--export-all-symbols")
endif()
target_link_libraries(${EXECUTABLE_NAME} ${CMAKE_DL_LIBS})
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
set(RELEASE_DRIVER_DIR "${RELEASE_DIR}/drivers")

add_library(the_driver_curses MODULE
    src/drivers/curses/cursesdriver.c
    src/drivers/curses/getch.c
)
set_target_properties(the_driver_curses PROPERTIES
    PREFIX ""
    OUTPUT_NAME "the_driver_curses"
)
target_link_libraries(the_driver_curses ${CURSES_LIBRARIES})
target_include_directories(the_driver_curses PRIVATE
    ${CMAKE_SOURCE_DIR}/src/drivers/curses
    ${CURSES_INCLUDE_DIRS}
)
if(USE_SDSLH)
    target_link_libraries(the_driver_curses DSLSH::editor)
endif()

add_library(the_driver_llm MODULE
    src/drivers/llm/headlessdriver.c
)
set_target_properties(the_driver_llm PROPERTIES
    PREFIX ""
    OUTPUT_NAME "the_driver_llm"
)
if(USE_SDSLH)
    target_link_libraries(the_driver_llm DSLSH::editor)
endif()

if(APPLE)
    target_link_options(the_driver_curses PRIVATE "-undefined" "dynamic_lookup")
    target_link_options(the_driver_llm PRIVATE "-undefined" "dynamic_lookup")
elseif(WIN32)
    target_link_libraries(the_driver_curses ${EXECUTABLE_NAME})
    target_link_libraries(the_driver_llm ${EXECUTABLE_NAME})
endif()

if(NOT WIN32)
    add_dependencies(${EXECUTABLE_NAME} the_driver_curses the_driver_llm)
endif()

foreach(THE_DRIVER_MODULE the_driver_curses the_driver_llm)
    add_custom_command(TARGET ${THE_DRIVER_MODULE} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DRIVER_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${THE_DRIVER_MODULE}>
                ${RELEASE_DRIVER_DIR}/$<TARGET_FILE_NAME:${THE_DRIVER_MODULE}>
        COMMENT "Copying ${THE_DRIVER_MODULE} to ${RELEASE_DRIVER_DIR}"
    )
endforeach()

add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${EXECUTABLE_NAME}> ${RELEASE_DIR}/${EXECUTABLE_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/doc/THE_Help.txt ${RELEASE_DIR}/THE_Help.txt
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}/batch-md-rexx
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/examples ${RELEASE_DIR}/batch-md-rexx/examples
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/syntax ${RELEASE_DIR}/syntax
    COMMENT "Copying executable and resources to ${RELEASE_DIR}"
)

set(THE_RUNTIME_PROFILE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/profile.the
    ${CMAKE_CURRENT_SOURCE_DIR}/profile_crexx.the
    ${CMAKE_CURRENT_SOURCE_DIR}/crexxcompile.the
    ${CMAKE_CURRENT_SOURCE_DIR}/crexxrun.the
)
set(THE_COPIED_RUNTIME_PROFILE_OUTPUTS)
foreach(THE_RUNTIME_PROFILE_SOURCE IN LISTS THE_RUNTIME_PROFILE_SOURCES)
    get_filename_component(THE_RUNTIME_PROFILE_BASENAME "${THE_RUNTIME_PROFILE_SOURCE}" NAME)
    set(THE_RUNTIME_PROFILE_OUTPUT "${RELEASE_DIR}/${THE_RUNTIME_PROFILE_BASENAME}")
    add_custom_command(
        OUTPUT ${THE_RUNTIME_PROFILE_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${THE_RUNTIME_PROFILE_SOURCE} ${THE_RUNTIME_PROFILE_OUTPUT}
        DEPENDS ${THE_RUNTIME_PROFILE_SOURCE}
        COMMENT "Copying runtime profile ${THE_RUNTIME_PROFILE_BASENAME} to ${RELEASE_DIR}"
    )
    list(APPEND THE_COPIED_RUNTIME_PROFILE_OUTPUTS ${THE_RUNTIME_PROFILE_OUTPUT})
endforeach()
add_custom_target(copy_runtime_profiles ALL
    DEPENDS ${THE_COPIED_RUNTIME_PROFILE_OUTPUTS}
)
add_dependencies(${EXECUTABLE_NAME} copy_runtime_profiles)

set(THE_BATCH_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/batch-md-rexx.crexx
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/render-html.crexx
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/highlight.crexx
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/highlight-source-profile.the
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/validate.crexx
)
set(THE_COPIED_BATCH_RUNTIME_OUTPUTS)
foreach(THE_BATCH_RUNTIME_SOURCE IN LISTS THE_BATCH_RUNTIME_SOURCES)
    get_filename_component(THE_BATCH_RUNTIME_BASENAME "${THE_BATCH_RUNTIME_SOURCE}" NAME)
    set(THE_BATCH_RUNTIME_OUTPUT "${RELEASE_DIR}/batch-md-rexx/${THE_BATCH_RUNTIME_BASENAME}")
    add_custom_command(
        OUTPUT ${THE_BATCH_RUNTIME_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}/batch-md-rexx
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${THE_BATCH_RUNTIME_SOURCE} ${THE_BATCH_RUNTIME_OUTPUT}
        DEPENDS ${THE_BATCH_RUNTIME_SOURCE}
        COMMENT "Copying batch runtime source ${THE_BATCH_RUNTIME_BASENAME} to ${RELEASE_DIR}/batch-md-rexx"
    )
    list(APPEND THE_COPIED_BATCH_RUNTIME_OUTPUTS ${THE_BATCH_RUNTIME_OUTPUT})
endforeach()
add_custom_target(copy_batch_runtime_sources ALL
    DEPENDS ${THE_COPIED_BATCH_RUNTIME_OUTPUTS}
)
add_dependencies(${EXECUTABLE_NAME} copy_batch_runtime_sources)

set(THE_PRECOMPILED_BATCH_RUNTIME_OUTPUTS)
if(CREXX_DRIVER_EXECUTABLE)
    set(THE_BATCH_PRECOMPILE_DIR "${CMAKE_CURRENT_BINARY_DIR}/batch-md-rexx-precompiled")
    foreach(THE_BATCH_PRECOMPILE_NAME IN ITEMS batch-md-rexx render-html)
        set(THE_BATCH_PRECOMPILE_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/${THE_BATCH_PRECOMPILE_NAME}.crexx"
        )
        set(THE_BATCH_PRECOMPILE_COPY
            "${THE_BATCH_PRECOMPILE_DIR}/${THE_BATCH_PRECOMPILE_NAME}.crexx"
        )
        set(THE_BATCH_PRECOMPILE_RXBIN
            "${THE_BATCH_PRECOMPILE_DIR}/${THE_BATCH_PRECOMPILE_NAME}.rxbin"
        )
        set(THE_BATCH_RELEASE_RXBIN
            "${RELEASE_DIR}/batch-md-rexx/${THE_BATCH_PRECOMPILE_NAME}.rxbin"
        )
        add_custom_command(
            OUTPUT ${THE_BATCH_RELEASE_RXBIN}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${THE_BATCH_PRECOMPILE_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}/batch-md-rexx
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${THE_BATCH_PRECOMPILE_SOURCE} ${THE_BATCH_PRECOMPILE_COPY}
            COMMAND ${CREXX_DRIVER_EXECUTABLE}
                    ${THE_BATCH_PRECOMPILE_COPY} -keep -noexec -nocolour
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${THE_BATCH_PRECOMPILE_RXBIN} ${THE_BATCH_RELEASE_RXBIN}
            DEPENDS
                ${THE_BATCH_PRECOMPILE_SOURCE}
                ${CREXX_DRIVER_EXECUTABLE}
            BYPRODUCTS
                ${THE_BATCH_PRECOMPILE_COPY}
                ${THE_BATCH_PRECOMPILE_DIR}/${THE_BATCH_PRECOMPILE_NAME}.rxas
                ${THE_BATCH_PRECOMPILE_RXBIN}
            COMMENT "Precompiling ${THE_BATCH_PRECOMPILE_NAME}.crexx for batch runtime"
            VERBATIM
        )
        list(APPEND THE_PRECOMPILED_BATCH_RUNTIME_OUTPUTS ${THE_BATCH_RELEASE_RXBIN})
    endforeach()
    add_custom_target(precompile_batch_runtime ALL
        DEPENDS ${THE_PRECOMPILED_BATCH_RUNTIME_OUTPUTS}
    )
    add_dependencies(${EXECUTABLE_NAME} precompile_batch_runtime)
endif()

set(THE_BATCH_LAUNCHER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/the-batch-md-rexx
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/the-highlight-source
)
set(THE_COPIED_BATCH_LAUNCHER_OUTPUTS)
foreach(THE_BATCH_LAUNCHER_SOURCE IN LISTS THE_BATCH_LAUNCHER_SOURCES)
    get_filename_component(THE_BATCH_LAUNCHER_BASENAME "${THE_BATCH_LAUNCHER_SOURCE}" NAME)
    set(THE_BATCH_LAUNCHER_OUTPUT "${RELEASE_DIR}/${THE_BATCH_LAUNCHER_BASENAME}")
    add_custom_command(
        OUTPUT ${THE_BATCH_LAUNCHER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${THE_BATCH_LAUNCHER_SOURCE} ${THE_BATCH_LAUNCHER_OUTPUT}
        DEPENDS ${THE_BATCH_LAUNCHER_SOURCE}
        COMMENT "Copying batch launcher ${THE_BATCH_LAUNCHER_BASENAME} to ${RELEASE_DIR}"
    )
    list(APPEND THE_COPIED_BATCH_LAUNCHER_OUTPUTS ${THE_BATCH_LAUNCHER_OUTPUT})
endforeach()
add_custom_target(copy_batch_launchers ALL
    DEPENDS ${THE_COPIED_BATCH_LAUNCHER_OUTPUTS}
)
add_dependencies(${EXECUTABLE_NAME} copy_batch_launchers)

file(GLOB_RECURSE THE_BATCH_TEMPLATE_SOURCES CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/templates/*"
)
set(THE_BATCH_TEMPLATE_STAMP "${RELEASE_DIR}/batch-md-rexx/templates/.copy-stamp")
add_custom_command(
    OUTPUT ${THE_BATCH_TEMPLATE_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}/batch-md-rexx/templates
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/templates
            ${RELEASE_DIR}/batch-md-rexx/templates
    COMMAND ${CMAKE_COMMAND} -E touch ${THE_BATCH_TEMPLATE_STAMP}
    DEPENDS ${THE_BATCH_TEMPLATE_SOURCES}
    COMMENT "Copying batch renderer templates to ${RELEASE_DIR}/batch-md-rexx/templates"
)
add_custom_target(copy_batch_templates ALL
    DEPENDS ${THE_BATCH_TEMPLATE_STAMP}
)
add_dependencies(${EXECUTABLE_NAME} copy_batch_templates)

set(THE_SYNTAX_HIGHLIGHTING_DOC_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/doc/syntax-highlighting.md
)
set(THE_SYNTAX_HIGHLIGHTING_DOC_OUTPUT
    ${RELEASE_DIR}/doc/syntax-highlighting.md
)
add_custom_command(
    OUTPUT ${THE_SYNTAX_HIGHLIGHTING_DOC_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}/doc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${THE_SYNTAX_HIGHLIGHTING_DOC_SOURCE}
            ${THE_SYNTAX_HIGHLIGHTING_DOC_OUTPUT}
    DEPENDS ${THE_SYNTAX_HIGHLIGHTING_DOC_SOURCE}
    COMMENT "Copying syntax-highlighting user guide to ${RELEASE_DIR}/doc"
)
add_custom_target(copy_end_user_docs ALL
    DEPENDS ${THE_SYNTAX_HIGHLIGHTING_DOC_OUTPUT}
)
add_dependencies(${EXECUTABLE_NAME} copy_end_user_docs)

if(USE_UTF8 AND THE_SYSTEM_PROFILE_SOURCES)
    set(THE_COPIED_SYSTEM_PROFILE_OUTPUTS)
    foreach(THE_SYSTEM_PROFILE_SOURCE IN LISTS THE_SYSTEM_PROFILE_SOURCES)
        get_filename_component(THE_SYSTEM_PROFILE_BASENAME "${THE_SYSTEM_PROFILE_SOURCE}" NAME)
        set(THE_SYSTEM_PROFILE_OUTPUT "${RELEASE_DIR}/${THE_SYSTEM_PROFILE_BASENAME}")
        add_custom_command(
            OUTPUT ${THE_SYSTEM_PROFILE_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${THE_SYSTEM_PROFILE_SOURCE} ${THE_SYSTEM_PROFILE_OUTPUT}
            DEPENDS ${THE_SYSTEM_PROFILE_SOURCE}
            COMMENT "Copying UTF system profile ${THE_SYSTEM_PROFILE_BASENAME} to ${RELEASE_DIR}"
        )
        list(APPEND THE_COPIED_SYSTEM_PROFILE_OUTPUTS ${THE_SYSTEM_PROFILE_OUTPUT})
    endforeach()
    add_custom_target(copy_utf_system_profiles ALL
        DEPENDS ${THE_COPIED_SYSTEM_PROFILE_OUTPUTS}
    )
endif()

if(WIN32)
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${CURSES_LIBRARIES}> ${RELEASE_DIR}/
        COMMENT "Copying PDCurses DLL to ${RELEASE_DIR}"
    )
endif()

# Install Targets
install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)
install(TARGETS the_driver_curses the_driver_llm
    LIBRARY DESTINATION lib/the/drivers
    RUNTIME DESTINATION lib/the/drivers)
if(WIN32)
    install(FILES $<TARGET_FILE:${CURSES_LIBRARIES}> DESTINATION bin)
endif()
install(PROGRAMS tools/batch-md-rexx/the-batch-md-rexx DESTINATION bin)
install(PROGRAMS tools/batch-md-rexx/the-highlight-source DESTINATION bin)
if(WIN32)
    install(PROGRAMS tools/the-env.ps1 DESTINATION bin)
endif()
install(FILES doc/THE_Help.txt profile.the profile_crexx.the crexxcompile.the crexxrun.the DESTINATION share/the)
install(FILES doc/syntax-highlighting.md DESTINATION share/doc/the)
install(FILES
    tools/batch-md-rexx/batch-md-rexx.crexx
    tools/batch-md-rexx/render-html.crexx
    tools/batch-md-rexx/highlight.crexx
    tools/batch-md-rexx/highlight-source-profile.the
    tools/batch-md-rexx/validate.crexx
    DESTINATION share/the/batch-md-rexx
)
if(THE_PRECOMPILED_BATCH_RUNTIME_OUTPUTS)
    install(FILES ${THE_PRECOMPILED_BATCH_RUNTIME_OUTPUTS}
        DESTINATION share/the/batch-md-rexx
    )
endif()
if(USE_UTF8 AND THE_SYSTEM_PROFILE_SOURCES)
    install(FILES ${THE_SYSTEM_PROFILE_SOURCES} DESTINATION share/the)
endif()
install(DIRECTORY syntax/ DESTINATION share/the/syntax)

# --- Automated Testing ---
enable_testing()
set(THE_BASH_HINTS)
if(WIN32)
    list(APPEND THE_BASH_HINTS
        "$ENV{ProgramFiles}/Git/bin"
        "$ENV{ProgramFiles}/Git/usr/bin"
        "$ENV{ProgramW6432}/Git/bin"
        "$ENV{ProgramW6432}/Git/usr/bin"
        "C:/Program Files/Git/bin"
        "C:/Program Files/Git/usr/bin"
        "C:/Program Files (x86)/Git/bin"
        "C:/Program Files (x86)/Git/usr/bin"
        "C:/msys64/usr/bin"
    )
endif()
find_program(THE_BASH_EXECUTABLE
    NAMES bash bash.exe
    HINTS ${THE_BASH_HINTS}
)
if(THE_BASH_EXECUTABLE)
    set(THE_BASH_COMMAND "${THE_BASH_EXECUTABLE}")
else()
    set(THE_BASH_COMMAND bash)
endif()
message(STATUS "Using Bash for tests: ${THE_BASH_COMMAND}")
set(THE_CTEST_RUNTIME_ENV
    "THE_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    "THE_BIN=$<TARGET_FILE:${EXECUTABLE_NAME}>"
    "THE_HOME_DIR=${RELEASE_DIR}"
    "THE_DRIVER_PATH=${CMAKE_CURRENT_BINARY_DIR}"
)
if(THE_USE_CREXX)
    list(APPEND THE_CTEST_RUNTIME_ENV
        "THE_CREXX_RXC=${CREXX_RXC_EXECUTABLE}"
        "THE_CREXX_RXAS=${CREXX_RXAS_EXECUTABLE}"
        "THE_CREXX_IMPORT_DIR=${CREXX_IMPORT_DIR}"
        "THE_CREXX_LIBRARY_RXBIN=${CREXX_LIBRARY_RXBIN}"
        "THE_BATCH_HIGHLIGHT_PROFILE=${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/highlight-source-profile.the"
        "CREXX=${CREXX_IMPORT_DIR}/crexx${CMAKE_EXECUTABLE_SUFFIX}"
    )
endif()
find_program(THE_RG_EXECUTABLE NAMES rg rg.exe)
set(THE_CTEST_PATH_DIRS
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${RELEASE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/src/PDCursesMod/wincon"
)
if(THE_USE_CREXX)
    list(APPEND THE_CTEST_PATH_DIRS
        "${CREXX_IMPORT_DIR}"
        "${CREXXSAA_LIBRARY_DIR}"
    )
endif()
if(THE_BASH_EXECUTABLE)
    get_filename_component(THE_BASH_DIR "${THE_BASH_EXECUTABLE}" DIRECTORY)
    get_filename_component(THE_BASH_ROOT "${THE_BASH_DIR}" DIRECTORY)
    list(APPEND THE_CTEST_PATH_DIRS
        "${THE_BASH_DIR}"
        "${THE_BASH_ROOT}/usr/bin"
    )
endif()
if(THE_RG_EXECUTABLE)
    get_filename_component(THE_RG_DIR "${THE_RG_EXECUTABLE}" DIRECTORY)
    list(APPEND THE_CTEST_PATH_DIRS "${THE_RG_DIR}")
endif()
if(WIN32)
    set(THE_CTEST_RUNTIME_PATH "$ENV{PATH}")
    foreach(THE_CTEST_PATH_DIR IN LISTS THE_CTEST_PATH_DIRS)
        if(THE_CTEST_PATH_DIR)
            file(TO_NATIVE_PATH "${THE_CTEST_PATH_DIR}" THE_CTEST_PATH_DIR_NATIVE)
            set(THE_CTEST_RUNTIME_PATH "${THE_CTEST_PATH_DIR_NATIVE};${THE_CTEST_RUNTIME_PATH}")
        endif()
    endforeach()
    string(REPLACE ";" "\\;" THE_CTEST_RUNTIME_PATH "${THE_CTEST_RUNTIME_PATH}")
    list(APPEND THE_CTEST_RUNTIME_ENV "PATH=${THE_CTEST_RUNTIME_PATH}")
endif()
add_test(NAME test_curses_boundary
    COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curses_boundary.sh
            ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(test_curses_boundary PROPERTIES
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}")
add_test(NAME test_curses_boundary_inventory
    COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/inventory_direct_curses.sh
            --fail-on-new
            ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(test_curses_boundary_inventory PROPERTIES
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}")
add_custom_target(curses_boundary_inventory
    COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/inventory_direct_curses.sh
            --fail-on-new
            ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running direct curses inventory ratchet")
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
add_executable(test_keycodes tests/test_keycodes.c)
target_include_directories(test_keycodes PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME test_keycodes COMMAND test_keycodes)
add_executable(test_render_attrs tests/test_render_attrs.c src/thecolour.c)
target_include_directories(test_render_attrs PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME test_render_attrs COMMAND test_render_attrs)
add_executable(test_curses_keymap tests/test_curses_keymap.c)
target_include_directories(test_curses_keymap PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/drivers/curses
    ${CURSES_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(test_curses_keymap ${CURSES_LIBRARIES})
add_test(NAME test_curses_keymap COMMAND test_curses_keymap)
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
add_executable(test_headlessdriver
    tests/test_headlessdriver.c
    src/driverlayout.c
    src/drivers/llm/headlessdriver.c
    src/inputevent.c
    src/rendercell.c
    src/thedriver.c
    src/logcursor.c
    src/textpos.c
    src/utfcluster.c
    src/utflayout.c
    src/utfterm.c
)
target_include_directories(test_headlessdriver PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(test_headlessdriver PRIVATE THE_DRIVER_ENABLE_HEADLESS=1)
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_headlessdriver ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_headlessdriver PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_headlessdriver COMMAND test_headlessdriver)
add_test(NAME test_headlessdriver_no_curses
    COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_no_curses_symbols.sh
            $<TARGET_FILE:test_headlessdriver>)
set_tests_properties(test_headlessdriver_no_curses PROPERTIES
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}")
add_executable(test_headlessdriver_curses_default
    tests/test_headlessdriver_curses_default.c
    src/inputevent.c
    src/thedriver.c
)
target_include_directories(test_headlessdriver_curses_default PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(test_headlessdriver_curses_default PRIVATE THE_DRIVER_ENABLE_CURSES=1)
add_dependencies(test_headlessdriver test_headlessdriver_curses_default)
add_test(NAME test_headlessdriver_curses_default COMMAND test_headlessdriver_curses_default)
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
add_executable(test_llmdriver tests/test_llmdriver.c src/llm/llmdriver.c src/inputevent.c src/uidriver.c src/logcursor.c src/textpos.c)
target_include_directories(test_llmdriver PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8)
    target_sources(test_llmdriver PRIVATE src/rendercell.c src/utfterm.c src/utflayout.c src/utfcluster.c)
endif()
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_llmdriver ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_llmdriver PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_llmdriver COMMAND test_llmdriver)
add_executable(test_virtual_screen tests/test_virtual_screen.c src/llm/llmdriver.c src/inputevent.c src/uidriver.c src/logcursor.c src/textpos.c)
target_include_directories(test_virtual_screen PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8)
    target_sources(test_virtual_screen PRIVATE src/rendercell.c src/utfterm.c src/utflayout.c src/utfcluster.c)
endif()
if(USE_UTF8 AND USE_UTF8PROC)
    target_link_libraries(test_virtual_screen ${THE_UTF8PROC_TARGET})
    target_compile_definitions(test_virtual_screen PRIVATE USE_UTF8PROC=1)
endif()
add_test(NAME test_virtual_screen COMMAND test_virtual_screen)
add_test(NAME test_the_llm_full_runtime
    COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_the_llm_full_runtime.sh
            $<TARGET_FILE:the>)
set_tests_properties(test_the_llm_full_runtime PROPERTIES
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}")
add_test(NAME test_rgb_colour_logical
    COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_rgb_colour_logical.sh
            $<TARGET_FILE:the>)
set_tests_properties(test_rgb_colour_logical PROPERTIES
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}")
add_test(NAME test_the_llm_parser_diagnostics
    COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_the_llm_parser_diagnostics.sh
            $<TARGET_FILE:the>)
set_tests_properties(test_the_llm_parser_diagnostics PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}")
add_test(NAME test_the_llm_profile_crexx
    COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_the_llm_profile_crexx.sh
            $<TARGET_FILE:the>)
set_tests_properties(test_the_llm_profile_crexx PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}")
add_test(NAME test_driver_modules
    COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_driver_modules.sh
            $<TARGET_FILE:the>
            $<TARGET_FILE:the_driver_curses>
            $<TARGET_FILE:the_driver_llm>)
set_tests_properties(test_driver_modules PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 20
    RESOURCE_LOCK the_pty_editor
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}")
add_executable(test_llmruntime tests/test_llmruntime.c src/llm/llmruntime.c src/llm/llmdriver.c src/parserdiagnostics.c src/inputevent.c src/screenframe.c src/uidriver.c src/logcursor.c src/textpos.c)
target_include_directories(test_llmruntime PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
if(USE_UTF8)
    target_sources(test_llmruntime PRIVATE src/rendercell.c src/utfterm.c src/utflayout.c src/utfcluster.c)
endif()
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

    add_executable(test_utfcluster tests/test_utfcluster.c src/utfcluster.c src/textpos.c)
    target_include_directories(test_utfcluster PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utfcluster ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utfcluster PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utfcluster COMMAND test_utfcluster)

    add_executable(test_textedit tests/test_textedit.c src/textedit.c src/textpos.c src/utfterm.c src/utfcluster.c)
    target_include_directories(test_textedit PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_textedit ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_textedit PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_textedit COMMAND test_textedit)

    add_executable(test_utfinput tests/test_utfinput.c src/utfinput.c src/textpos.c)
    target_include_directories(test_utfinput PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utfinput ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utfinput PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utfinput COMMAND test_utfinput)

    add_executable(test_utfterm tests/test_utfterm.c src/utfterm.c src/utfcluster.c src/textpos.c)
    target_include_directories(test_utfterm PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utfterm ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utfterm PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utfterm
        COMMAND test_utfterm
            ${CMAKE_CURRENT_SOURCE_DIR}/system-osx.the)

    add_executable(test_utflayout tests/test_utflayout.c src/utflayout.c src/utfterm.c src/utfcluster.c src/textpos.c)
    target_include_directories(test_utflayout PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utflayout ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utflayout PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utflayout COMMAND test_utflayout)

    add_executable(test_utfrepair tests/test_utfrepair.c src/utfrepair.c src/utfterm.c src/utfcluster.c src/textpos.c)
    target_include_directories(test_utfrepair PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
    if(USE_UTF8PROC)
        target_link_libraries(test_utfrepair ${THE_UTF8PROC_TARGET})
        target_compile_definitions(test_utfrepair PRIVATE USE_UTF8PROC=1)
    endif()
    add_test(NAME test_utfrepair COMMAND test_utfrepair)

    add_test(NAME test_system_profile COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_system_profile.sh)
    set_tests_properties(test_system_profile PROPERTIES
        SKIP_RETURN_CODE 77
        ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
    )
endif()
add_test(NAME test_sdslh_integration COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sdslh_integration.sh)
set_tests_properties(test_sdslh_integration PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_sdslh_multifile_lifecycle COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sdslh_multifile_lifecycle.sh)
set_tests_properties(test_sdslh_multifile_lifecycle PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_stylespans_extract COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stylespans_extract.sh)
set_tests_properties(test_stylespans_extract PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_file_ecolor_inheritance COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_file_ecolor_inheritance.sh)
set_tests_properties(test_file_ecolor_inheritance PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_message_history COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_message_history.sh)
set_tests_properties(test_message_history PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_format COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_format.sh)
set_tests_properties(test_batch_md_rexx_format PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_scan COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_scan.sh)
set_tests_properties(test_batch_md_rexx_scan PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_render_html COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_render_html.sh)
set_tests_properties(test_batch_md_rexx_render_html PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_execute COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_execute.sh)
set_tests_properties(test_batch_md_rexx_execute PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_validate COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_validate.sh)
set_tests_properties(test_batch_md_rexx_validate PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_output_render COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_output_render.sh)
set_tests_properties(test_batch_md_rexx_output_render PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_runner COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_runner.sh)
set_tests_properties(test_batch_md_rexx_runner PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_tex COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_tex.sh)
set_tests_properties(test_batch_md_rexx_tex PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_highlight COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_highlight.sh)
set_tests_properties(test_batch_md_rexx_highlight PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
add_test(NAME test_batch_md_rexx_highlight_source COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tools/batch-md-rexx/tests/test_highlight_source.sh)
set_tests_properties(test_batch_md_rexx_highlight_source PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
)
# These tests launch the same cREXX renderer sources and DSLSH parser tools.
# Concurrent full-suite runs can otherwise produce an undiagnosed driver rc 255
# even though the affected test passes immediately when run alone.
set_tests_properties(
    test_batch_md_rexx_format
    test_batch_md_rexx_scan
    test_batch_md_rexx_render_html
    test_batch_md_rexx_execute
    test_batch_md_rexx_validate
    test_batch_md_rexx_output_render
    test_batch_md_rexx_runner
    test_batch_md_rexx_tex
    test_batch_md_rexx_highlight
    test_batch_md_rexx_highlight_source
    PROPERTIES RESOURCE_LOCK the_batch_md_rexx_toolchain
)
if(THE_USE_CREXX)
    add_test(NAME test_field_queries COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_field_queries.sh)
    set_tests_properties(test_field_queries PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 10
        RESOURCE_LOCK the_pty_editor
        ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
    )
    if(USE_UTF8)
        add_test(NAME test_field_queries_utf COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_field_queries.sh --utf)
        set_tests_properties(test_field_queries_utf PROPERTIES
            SKIP_RETURN_CODE 77
            TIMEOUT 10
            RESOURCE_LOCK the_pty_editor
            DEPENDS test_field_queries
            ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
        )
    endif()
    add_test(NAME test_normal_area_queries COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_normal_area_queries.sh)
    set_tests_properties(test_normal_area_queries PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 10
        RESOURCE_LOCK the_pty_editor
        ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
    )
    add_test(NAME test_curses_text_entry
        COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_curses_text_entry.sh
                $<TARGET_FILE:the>)
    set_tests_properties(test_curses_text_entry PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 10
        RESOURCE_LOCK the_pty_editor
        DEPENDS test_normal_area_queries
        ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
    )
    add_test(NAME test_sos_navigation_queries COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sos_navigation_queries.sh)
    set_tests_properties(test_sos_navigation_queries PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 10
        RESOURCE_LOCK the_pty_editor
        DEPENDS test_curses_text_entry
        ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
    )
    add_test(NAME test_sos_logical_edit_queries COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sos_logical_edit_queries.sh)
    set_tests_properties(test_sos_logical_edit_queries PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 10
        RESOURCE_LOCK the_pty_editor
        DEPENDS test_sos_navigation_queries
        ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
    )
    add_test(NAME test_selective_change_prompt COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_selective_change_prompt.sh)
    set_tests_properties(test_selective_change_prompt PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 15
        RESOURCE_LOCK the_pty_editor
        DEPENDS test_sos_logical_edit_queries
        ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
    )
    add_test(NAME test_crexx_profile COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_crexx_profile.sh)
    set_tests_properties(test_crexx_profile PROPERTIES
        SKIP_RETURN_CODE 77
        ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
    )
    add_test(NAME test_batch_profile_status COMMAND ${THE_BASH_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_batch_profile_status.sh)
    set_tests_properties(test_batch_profile_status PROPERTIES
        SKIP_RETURN_CODE 77
        ENVIRONMENT "${THE_CTEST_RUNTIME_ENV}"
    )
endif()
