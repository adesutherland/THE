set(THE_WASM_RUNNER_DIRECTORY "" CACHE PATH "Prepared interpreter build's runner directory")
include("${CMAKE_SOURCE_DIR}/cmake/ClassicInterpreterCohort.cmake")
foreach(asset crxthe_rxvm.mjs crxthe_rxvm.wasm llcbrowser.rxbin interpreter.json)
    if(NOT EXISTS "${THE_WASM_RUNNER_DIRECTORY}/${asset}")
        message(FATAL_ERROR "Prepare THE_BUILD_WASM_RUNNER first; missing ${asset}")
    endif()
endforeach()
file(READ "${THE_WASM_RUNNER_DIRECTORY}/interpreter.json" interpreter_metadata)
if(NOT interpreter_metadata MATCHES "${THE_CREXX_INTERPRETER_REVISION}"
   OR NOT interpreter_metadata MATCHES "${THE_WASM_INTERPRETER_SHA256}")
    message(FATAL_ERROR "Prepared runner metadata differs from the qualified interpreter cohort")
endif()
file(SHA256 "${THE_WASM_RUNNER_DIRECTORY}/llcbrowser.rxbin" interpreter_image_hash)
if(NOT interpreter_image_hash STREQUAL THE_WASM_INTERPRETER_SHA256)
    message(FATAL_ERROR "Prepared runner bytecode differs from its qualified checksum")
endif()
add_compile_definitions(THE_WASM_INTERPRETER=1)
add_custom_target(the_wasm_interpreter_assets
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${THE_WASM_SITE}/vm/runner" "${THE_WASM_SITE}/examples"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/web/vm" "${THE_WASM_SITE}/vm"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${THE_WASM_RUNNER_DIRECTORY}" "${THE_WASM_SITE}/vm/runner"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/examples/browser-interpreter.rexx"
        "${THE_WASM_SITE}/examples/RexxRunner.rexx"
    DEPENDS "${THE_WASM_SITE}/.build-stamp" VERBATIM)
list(APPEND THE_WASM_PACKAGE_ARGUMENTS --interpreter)
if(THE_WASM_HIGHLIGHT)
    add_dependencies(the_wasm_interpreter_assets the_wasm_classic_assets)
endif()
