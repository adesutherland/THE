# The first browser parser connection uses a prepared, self-contained image.
# Compilation/generation stays in cREXX/DSLSH; this build never regenerates a grammar.
set(THE_DSLSH_SOURCE "${CMAKE_SOURCE_DIR}/../DSL-Syntax-Highlighter" CACHE PATH "Pinned DSLSH codec source")
set(THE_WASM_CLASSIC_IMAGE "" CACHE FILEPATH "Prepared linked dslsh-classic.rxbin")
set(THE_WASM_CLASSIC_SHA256 "e549b76afa654e4a4264f8183de0c7435fe93688337eb13635c47a0c252f7e59"
    CACHE STRING "Expected checksum of the reviewed Classic image")
set(THE_DSLSH_CLASSIC_REVISION "0ea4db815abd55cdc5db4a5c41539d7d920e9cb3")
set(THE_CREXX_CLASSIC_REVISION "67a5e928cddf569595961267bee5d25bf92f267f")
include("${CMAKE_SOURCE_DIR}/cmake/CrexxWasm.cmake")
if(NOT THE_CREXX_REVISION STREQUAL THE_CREXX_CLASSIC_REVISION)
    message(FATAL_ERROR "The Classic highlighter cohort requires cREXX ${THE_CREXX_CLASSIC_REVISION}")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD WORKING_DIRECTORY "${THE_DSLSH_SOURCE}"
    OUTPUT_VARIABLE dslsh_revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT dslsh_revision STREQUAL THE_DSLSH_CLASSIC_REVISION)
    message(FATAL_ERROR "The Classic highlighter requires DSLSH ${THE_DSLSH_CLASSIC_REVISION}")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" diff --quiet HEAD -- codebuffer crexx
    WORKING_DIRECTORY "${THE_DSLSH_SOURCE}" RESULT_VARIABLE dslsh_modified)
if(NOT dslsh_modified EQUAL 0 OR NOT EXISTS "${THE_WASM_CLASSIC_IMAGE}")
    message(FATAL_ERROR "Supply clean DSLSH sources and a prepared THE_WASM_CLASSIC_IMAGE")
endif()
file(SHA256 "${THE_WASM_CLASSIC_IMAGE}" classic_actual_sha256)
if(NOT classic_actual_sha256 STREQUAL THE_WASM_CLASSIC_SHA256)
    message(FATAL_ERROR "Classic image differs from the reviewed checksum; qualify and pin its new SHA256 explicitly")
endif()
the_add_wasm_vm()
add_compile_definitions(THE_WASM_HIGHLIGHT=1)
target_sources(crxthe_wasm PRIVATE src/wasm/highlighter.c
    "${THE_DSLSH_SOURCE}/codebuffer/serialization.c"
    "${THE_DSLSH_SOURCE}/codebuffer/thread_utils.c")
target_include_directories(crxthe_wasm PRIVATE "${THE_DSLSH_SOURCE}/codebuffer")
set(THE_WASM_CLASSIC_FILES "${CMAKE_BINARY_DIR}/classic-assets")
file(MAKE_DIRECTORY "${THE_WASM_CLASSIC_FILES}")
file(WRITE "${THE_WASM_CLASSIC_FILES}/classic.json"
    "{\"crexx\":\"${THE_CREXX_CLASSIC_REVISION}\",\"dslsh\":\"${THE_DSLSH_CLASSIC_REVISION}\",\"sha256\":\"${THE_WASM_CLASSIC_SHA256}\"}\n")
add_custom_target(the_wasm_classic_assets
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${THE_WASM_SITE}/vm/modules"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${THE_WASM_SITE}/examples"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/web/vm" "${THE_WASM_SITE}/vm"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:crxthe_rxvm>"
        "${CMAKE_BINARY_DIR}/crxthe_rxvm.wasm" "${THE_WASM_CLASSIC_FILES}/classic.json" "${THE_WASM_SITE}/vm"
    COMMAND "${CMAKE_COMMAND}" -E copy "${THE_WASM_CLASSIC_IMAGE}" "${THE_WASM_SITE}/vm/modules/dslsh-classic.rxbin"
    COMMAND "${CMAKE_COMMAND}" -E copy "${THE_DSLSH_SOURCE}/crexx/examples/classic.rexx" "${THE_WASM_SITE}/examples/ClassicDemo.rexx"
    DEPENDS crxthe_rxvm "${THE_WASM_SITE}/.build-stamp" "${THE_WASM_CLASSIC_IMAGE}"
    VERBATIM)
list(APPEND THE_WASM_PACKAGE_ARGUMENTS --dslsh-source "${THE_DSLSH_SOURCE}" --classic)
if(NOT THE_WASM_CREXX)
    list(APPEND THE_WASM_PACKAGE_ARGUMENTS --crexx-source "${THE_CREXX_SOURCE}" --crexx-revision "${THE_CREXX_REVISION}")
endif()
