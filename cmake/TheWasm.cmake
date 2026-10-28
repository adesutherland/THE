if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Use emcmake cmake with -DTHE_BUILD_WASM=ON")
endif()
if(NOT EMSCRIPTEN_VERSION VERSION_EQUAL "6.0.13")
    message(FATAL_ERROR "The browser build is pinned to Emscripten 6.0.13; found ${EMSCRIPTEN_VERSION}")
endif()

option(THE_WASM_CREXX "Run precompiled cREXX browser profiles and macros" OFF)
option(THE_WASM_HIGHLIGHT "Run the Level L Classic Rexx highlighter in a browser VM" OFF)
set(THE_WASM_COMPILER_DIRECTORY "" CACHE PATH "Prepared fresh-module browser compiler SDK")
option(THE_WASM_INTERPRETER "Bundle the Classic Rexx interpreter and split console" OFF)
if(THE_WASM_CREXX)
    set(THE_WASM_REXX_DEFAULT CREXX)
else()
    set(THE_WASM_REXX_DEFAULT NONE)
endif()
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_definitions(_GNU_SOURCE=1)
set(USE_UTF8 ON)
set(USE_UTF8PROC ON)
set(UNIX 1)
set(STDC_HEADERS 1)
foreach(feature HAVE_STDLIB_H HAVE_STDINT_H HAVE_STRING_H HAVE_MEMORY_H
        HAVE_STRINGS_H HAVE_UNISTD_H HAVE_ERRNO_H HAVE_CTYPE_H HAVE_WCTYPE_H
        HAVE_SYS_TYPES_H HAVE_SYS_STAT_H HAVE_SYS_TIME_H HAVE_SYS_WAIT_H
        HAVE_TIME_H HAVE_LOCALE_H HAVE_STDARG_H HAVE_INTTYPES_H HAVE_SELECT_H HAVE_SYS_SELECT_H HAVE_FCNTL_H HAVE_LIMITS_H
        HAVE_DIRENT_H HAVE_SIGNAL_H HAVE_SETLOCALE HAVE_REALPATH
        HAVE_STRCASECMP HAVE_READLINK HAVE_SELECT HAVE_GETCWD HAVE_SNPRINTF
        HAVE_VSNPRINTF HAVE_MKSTEMP HAVE_RENAME HAVE_LSTAT TIME_WITH_SYS_TIME)
    set(${feature} 1)
endforeach()
configure_file("${CMAKE_SOURCE_DIR}/config.h.in" "${CMAKE_BINARY_DIR}/config.h")
add_compile_definitions(HAVE_CONFIG_H USE_UTF8 USE_WIDE_CHAR USE_UTF8PROC=1
    NOREXX=1 THE_WASM=1 THE_DRIVER_ENABLE_WEB=1
    THE_HEADLESS_DRIVER_NO_MODULE_EXPORTS=1
    THE_DEFAULT_REXX_INTERPRETER="${THE_WASM_REXX_DEFAULT}"
    THE_PRODUCT_NAME="${THE_PRODUCT_NAME}" THE_VERSION="${THE_VERSION}"
    THE_VERSION_DATE="${THE_VERSION_DATE}" THE_VARIANT="wasm"
    THE_PLATFORM_NAME="wasm" THE_SYSTEM_PROFILE_NAME="system-wasm.the"
    THE_SYSTEM_PROFILE_DIR="/app" CAN_RESIZE=1 BUILD3001=1)
include_directories("${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/llm" "${CMAKE_SOURCE_DIR}/src/drivers/llm"
    "${CMAKE_SOURCE_DIR}/src/contrib")

include(FetchContent)
set(UTF8PROC_INSTALL OFF CACHE BOOL "" FORCE)
set(UTF8PROC_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(utf8proc
    GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
    GIT_TAG e5e799221b45bbb90f5fdc5c69b6b8dfbf017e78)
FetchContent_MakeAvailable(utf8proc)
include("${CMAKE_SOURCE_DIR}/cmake/TheSources.cmake")

if(THE_WASM_CREXX)
    include("${CMAKE_SOURCE_DIR}/cmake/CrexxWasm.cmake")
    if(THE_CREXX_REVISION STREQUAL "8e8cfff4d6109bce277befeb9dd598027d1deb77")
      add_compile_definitions(THE_TOOL_PROFILE_OUTPUT=1)
    endif()
    add_compile_definitions(THE_TOOL_COHORT="${THE_CREXX_REVISION}" USE_CREXX=1 THE_CREXX_STATIC=1
        THE_CREXX_IMPORT_DIR="/app" THE_CREXX_LIBRARY_RXBIN="/app/library.rxbin")
    add_library(the_wasm_saa STATIC "${THE_CREXX_SOURCE}/interpreter/crexxsaa.c")
    target_link_libraries(the_wasm_saa PUBLIC rxvm_single_embed)
    set(THE_WASM_RESOURCE_ARGUMENTS --crexx-bin "${THE_CREXX_HOST_BUILD}/bin")
    set(THE_WASM_PROFILE_DEPENDS quickfind.the quickopen.the quickline.the paneldemo.the
        "${THE_CREXX_HOST_BUILD}/bin/rxc" "${THE_CREXX_HOST_BUILD}/bin/rxas"
        "${THE_CREXX_HOST_BUILD}/bin/rxcexits.rxbin" "${THE_CREXX_HOST_BUILD}/bin/library.rxbin")
    set(THE_WASM_PACKAGE_ARGUMENTS --crexx-source "${THE_CREXX_SOURCE}" --crexx-revision "${THE_CREXX_REVISION}" --editor-address)
endif()

find_program(THE_WASM_PYTHON NAMES python3 REQUIRED)
find_program(THE_WASM_NPM NAMES npm REQUIRED)
set(THE_WASM_RESOURCES "${CMAKE_BINARY_DIR}/resources")
file(GLOB THE_WASM_SYNTAX CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/syntax/*")
set(THE_WASM_RESOURCE_STAMP "${CMAKE_BINARY_DIR}/resources.stamp")
add_custom_command(OUTPUT "${THE_WASM_RESOURCE_STAMP}"
    COMMAND "${THE_WASM_PYTHON}"
        "${CMAKE_SOURCE_DIR}/scripts/prepare-wasm-resources.py" "${THE_WASM_RESOURCES}" ${THE_WASM_RESOURCE_ARGUMENTS}
    COMMAND "${CMAKE_COMMAND}" -E touch "${THE_WASM_RESOURCE_STAMP}"
    DEPENDS scripts/prepare-wasm-resources.py profiles/default-profile.json
        doc/THE_Help.txt ${THE_WASM_SYNTAX} ${THE_WASM_PROFILE_DEPENDS}
    VERBATIM)
add_custom_target(the_wasm_resources DEPENDS "${THE_WASM_RESOURCE_STAMP}")

add_executable(crxthe_wasm ${THE_SOURCES}
    src/websession.c src/drivers/llm/headlessdriver.c
    src/drivers/wasm/wasmdriver.c src/tools_wasm.c)
target_link_libraries(crxthe_wasm PRIVATE utf8proc)
if(THE_WASM_CREXX)
    target_sources(crxthe_wasm PRIVATE src/wasm/crexxfs.c src/wasm/crexxresources.c)
    target_link_libraries(crxthe_wasm PRIVATE the_wasm_saa)
    target_link_options(crxthe_wasm PRIVATE -Wl,--wrap=fileexists)
endif()
add_dependencies(crxthe_wasm the_wasm_resources)
set_property(TARGET crxthe_wasm APPEND PROPERTY LINK_DEPENDS "${THE_WASM_RESOURCE_STAMP}")
set_target_properties(crxthe_wasm PROPERTIES SUFFIX ".mjs")
target_link_options(crxthe_wasm PRIVATE
    -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=1048576 -sSTACK_SIZE=1048576
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=worker,node
    "-sEXPORTED_FUNCTIONS=['_main','_the_wasm_main','_the_wasm_has_crexx','_the_wasm_has_highlighter','_the_wasm_has_interpreter','_setenv','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','FS','IDBFS','ENV','stringToUTF8']"
    -lidbfs.js "--preload-file=${THE_WASM_RESOURCES}@/app")

set(THE_WASM_SITE "${CMAKE_BINARY_DIR}/site")
file(GLOB_RECURSE THE_WASM_WEB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/web/src/*")
add_custom_command(OUTPUT "${THE_WASM_SITE}/.build-stamp"
    COMMAND "${THE_WASM_NPM}" ci
    COMMAND "${THE_WASM_NPM}" exec tsc -- --noEmit
    COMMAND "${CMAKE_COMMAND}" -E env THE_WEB_BACKEND=wasm
        "${THE_WASM_NPM}" run build -- --outDir "${THE_WASM_SITE}" --emptyOutDir
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${THE_WASM_SITE}/wasm"
    COMMAND "${CMAKE_COMMAND}" -E copy
        "$<TARGET_FILE:crxthe_wasm>" "${CMAKE_BINARY_DIR}/crxthe_wasm.wasm"
        "${CMAKE_BINARY_DIR}/crxthe_wasm.data" "${THE_WASM_SITE}/wasm"
    COMMAND "${CMAKE_COMMAND}" -E touch "${THE_WASM_SITE}/.build-stamp"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/web"
    DEPENDS crxthe_wasm ${THE_WASM_WEB_SOURCES}
        "${CMAKE_SOURCE_DIR}/web/vite.config.ts"
        "${CMAKE_SOURCE_DIR}/web/index.html" "${CMAKE_SOURCE_DIR}/web/tsconfig.json"
        "${CMAKE_SOURCE_DIR}/web/package.json" "${CMAKE_SOURCE_DIR}/web/package-lock.json"
    VERBATIM)
if(THE_WASM_HIGHLIGHT)
    include("${CMAKE_SOURCE_DIR}/cmake/TheWasmHighlight.cmake")
endif()
if(THE_WASM_INTERPRETER)
    include("${CMAKE_SOURCE_DIR}/cmake/TheWasmInterpreter.cmake")
endif()
add_custom_target(the_wasm_bundle ALL DEPENDS "${THE_WASM_SITE}/.build-stamp"
    COMMAND "${THE_WASM_PYTHON}" "${CMAKE_SOURCE_DIR}/scripts/package-wasm-site.py"
        "${THE_WASM_SITE}"
        "${THE_VERSION}" "${EMSCRIPTEN_VERSION}"
        --utf8-license "${utf8proc_SOURCE_DIR}/LICENSE.md"
        --emscripten-license "${EMSCRIPTEN_ROOT_PATH}/LICENSE" ${THE_WASM_PACKAGE_ARGUMENTS}
    VERBATIM)
if(THE_WASM_HIGHLIGHT)
    add_dependencies(the_wasm_bundle the_wasm_classic_assets)
endif()
if(THE_WASM_INTERPRETER)
    add_dependencies(the_wasm_bundle the_wasm_interpreter_assets)
endif()

if(THE_WASM_COMPILER_DIRECTORY)
 add_compile_definitions(THE_WASM_PROFILE_COMPILER=1)
 if(NOT THE_CREXX_REVISION STREQUAL "8e8cfff4d6109bce277befeb9dd598027d1deb77")
  message(FATAL_ERROR "The profile compiler requires the qualified 8e8cfff4 runtime SDK")
 endif()
 foreach(asset rxc_single.mjs rxc_single.wasm rxas_single.mjs rxas_single.wasm)
  if(NOT EXISTS "${THE_WASM_COMPILER_DIRECTORY}/${asset}")
   message(FATAL_ERROR "Browser compiler missing ${asset}")
  endif()
 endforeach()
 file(SHA256 "${THE_WASM_COMPILER_DIRECTORY}/rxc_single.wasm" rxc_hash)
 file(SHA256 "${THE_WASM_COMPILER_DIRECTORY}/rxas_single.wasm" rxas_hash)
 if(NOT rxc_hash STREQUAL "c2f3c5b76d2f1f982611c7338e4a3a30799337522bb86d098ad27e4b0ad1b8b0" OR NOT rxas_hash STREQUAL "1e8127835f8daeb0697bc966cdb2ec1b63c7b79c604bd8c6cbdba31de12f2c27")
  message(FATAL_ERROR "Profile compiler Wasm differs from its qualified module checksums")
 endif()
 file(WRITE "${CMAKE_BINARY_DIR}/compiler.json" "{\"version\":1,\"crexx\":\"${THE_CREXX_REVISION}\",\"protocol\":\"fresh-module-cli\",\"rxc_sha256\":\"${rxc_hash}\",\"rxas_sha256\":\"${rxas_hash}\"}\n")
 add_custom_target(the_wasm_compiler_assets
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${THE_WASM_SITE}/compiler"
  COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxc" -i "${THE_CREXX_HOST_BUILD}/bin" -o "${CMAKE_BINARY_DIR}/tool-example" "${CMAKE_SOURCE_DIR}/examples/tool-program.crexx"
  COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxas" -o "${CMAKE_BINARY_DIR}/tool-example" "${CMAKE_BINARY_DIR}/tool-example"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/tool-example.rxbin" "${THE_WASM_SITE}/examples/ToolHello.rxbin"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${THE_WASM_COMPILER_DIRECTORY}" "${THE_WASM_SITE}/compiler"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/compiler.json" "${THE_WASM_SITE}/compiler"
  COMMAND "${CMAKE_COMMAND}" -E copy "${THE_CREXX_HOST_BUILD}/bin/library.rxbin" "${THE_CREXX_HOST_BUILD}/bin/rxcexits.rxbin" "${THE_WASM_SITE}/compiler"
  DEPENDS "${THE_WASM_SITE}/.build-stamp" VERBATIM)
 add_dependencies(the_wasm_bundle the_wasm_compiler_assets)
endif()
