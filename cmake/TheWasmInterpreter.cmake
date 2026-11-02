set(THE_WASM_RUNNER_DIRECTORY "" CACHE PATH "Prepared interpreter build's runner directory")
include("${CMAKE_SOURCE_DIR}/cmake/ClassicInterpreterCohort.cmake")
if(THE_WASM_CREXX AND NOT THE_CREXX_REVISION STREQUAL THE_CREXX_INTERPRETER_REVISION)
    message(FATAL_ERROR "The editor and Classic Rexx runner require the same cREXX runtime cohort")
endif()
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
if(THE_WASM_CREXX)
    set(THE_WASM_RXBIN_EXAMPLE_COMMANDS
        COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxc" -i "${THE_CREXX_HOST_BUILD}/bin"
            -o "${CMAKE_BINARY_DIR}/tool-example" "${CMAKE_SOURCE_DIR}/examples/tool-program.crexx"
        COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxas" -o "${CMAKE_BINARY_DIR}/tool-example"
            "${CMAKE_BINARY_DIR}/tool-example"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/tool-example.rxbin"
            "${THE_WASM_SITE}/examples/ToolHello.rxbin"
        COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxc" -i "${THE_CREXX_HOST_BUILD}/bin"
            -o "${CMAKE_BINARY_DIR}/level-g-accumulator" "${CMAKE_SOURCE_DIR}/examples/LevelGAccumulator.crexx"
        COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxas" -o "${CMAKE_BINARY_DIR}/level-g-accumulator"
            "${CMAKE_BINARY_DIR}/level-g-accumulator"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/level-g-accumulator.rxbin"
            "${THE_WASM_SITE}/examples/LevelGAccumulator.rxbin"
        COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxc" -i "${THE_CREXX_HOST_BUILD}/bin"
            -o "${CMAKE_BINARY_DIR}/level-g-fibonacci" "${CMAKE_SOURCE_DIR}/examples/LevelGFibonacci.crexx"
        COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxas" -o "${CMAKE_BINARY_DIR}/level-g-fibonacci"
            "${CMAKE_BINARY_DIR}/level-g-fibonacci"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/level-g-fibonacci.rxbin"
            "${THE_WASM_SITE}/examples/LevelGFibonacci.rxbin"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_SOURCE_DIR}/examples/LevelGAccumulator.crexx"
            "${CMAKE_SOURCE_DIR}/examples/LevelGFibonacci.crexx"
            "${THE_WASM_SITE}/examples"
        COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxc" -i "${THE_CREXX_HOST_BUILD}/bin"
            -o "${CMAKE_BINARY_DIR}/source-file-probe" "${CMAKE_SOURCE_DIR}/tests/source-file-runner.crexx"
        COMMAND "${THE_CREXX_HOST_BUILD}/bin/rxas" -o "${CMAKE_BINARY_DIR}/source-file-probe"
            "${CMAKE_BINARY_DIR}/source-file-probe"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/source-file-probe.rxbin"
            "${THE_WASM_SITE}/examples/SourceFileProbe.rxbin")
endif()
add_custom_target(the_wasm_interpreter_assets
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${THE_WASM_SITE}/vm/runner" "${THE_WASM_SITE}/examples"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/web/vm" "${THE_WASM_SITE}/vm"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${THE_WASM_RUNNER_DIRECTORY}" "${THE_WASM_SITE}/vm/runner"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/web/vm/vm-worker.mjs"
        "${CMAKE_SOURCE_DIR}/web/vm/byte-input.mjs" "${CMAKE_SOURCE_DIR}/web/vm/source-file.mjs" "${THE_WASM_SITE}/vm/runner"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/examples/browser-interpreter.rexx"
        "${THE_WASM_SITE}/examples/RexxRunner.rexx"
    COMMAND "${CMAKE_COMMAND}" -E copy
        "${CMAKE_SOURCE_DIR}/examples/RexxWords.rexx"
        "${CMAKE_SOURCE_DIR}/examples/RexxLedger.rexx"
        "${THE_WASM_SITE}/examples"
    ${THE_WASM_RXBIN_EXAMPLE_COMMANDS}
    DEPENDS "${THE_WASM_SITE}/.build-stamp"
        "${CMAKE_SOURCE_DIR}/examples/RexxWords.rexx"
        "${CMAKE_SOURCE_DIR}/examples/RexxLedger.rexx"
        "${CMAKE_SOURCE_DIR}/examples/LevelGAccumulator.crexx"
        "${CMAKE_SOURCE_DIR}/examples/LevelGFibonacci.crexx"
    VERBATIM)
list(APPEND THE_WASM_PACKAGE_ARGUMENTS --interpreter)
if(THE_WASM_HIGHLIGHT)
    add_dependencies(the_wasm_interpreter_assets the_wasm_classic_assets)
endif()
