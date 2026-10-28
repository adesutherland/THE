# A separate build keeps interpreter providers out of the editor/parser cohort.
if(NOT EMSCRIPTEN OR NOT EMSCRIPTEN_VERSION VERSION_EQUAL "6.0.13")
    message(FATAL_ERROR "Use Emscripten 6.0.13 for the browser interpreter")
endif()
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
include("${CMAKE_SOURCE_DIR}/cmake/ClassicInterpreterCohort.cmake")
set(THE_CREXX_REVISION "${THE_CREXX_INTERPRETER_REVISION}" CACHE STRING "Pinned cREXX VM/tool source revision")
include("${CMAKE_SOURCE_DIR}/cmake/CrexxWasm.cmake")
if(NOT THE_CREXX_REVISION STREQUAL THE_CREXX_INTERPRETER_REVISION)
    message(FATAL_ERROR "The interpreter requires cREXX ${THE_CREXX_INTERPRETER_REVISION}")
endif()
if(NOT CREXX_VM_CLASSIC_FULL_PROVIDERS)
    message(FATAL_ERROR "The interpreter requires the full static Classic providers")
endif()
the_add_wasm_vm()
target_compile_definitions(crxthe_rxvm PRIVATE THE_WASM_RUNNER=1)
target_link_options(crxthe_rxvm PRIVATE
    "-sEXPORTED_FUNCTIONS=['_the_vm_create','_the_vm_destroy','_the_vm_load','_the_vm_run','_the_vm_interpret','_the_vm_result','_the_vm_error','_the_vm_program_rc','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','FS','UTF8ToString','HEAPU8','HEAPU32']")
set(THE_WASM_INTERPRETER_IMAGE "" CACHE FILEPATH "Prepared linked llcbrowser.rxbin")
set(THE_WASM_INTERPRETER_SHA256 "" CACHE STRING "Qualified interpreter image SHA256")
if(NOT EXISTS "${THE_WASM_INTERPRETER_IMAGE}" OR NOT THE_WASM_INTERPRETER_SHA256)
    message(FATAL_ERROR "Supply the prepared interpreter image and its qualified SHA256")
endif()
file(SHA256 "${THE_WASM_INTERPRETER_IMAGE}" interpreter_sha256)
if(NOT interpreter_sha256 STREQUAL THE_WASM_INTERPRETER_SHA256)
    message(FATAL_ERROR "Interpreter RXBIN checksum differs from its qualified image")
endif()
set(runner_site "${CMAKE_BINARY_DIR}/runner")
file(MAKE_DIRECTORY "${runner_site}")
file(WRITE "${runner_site}/interpreter.json"
    "{\"version\":1,\"crexx\":\"${THE_CREXX_REVISION}\",\"sha256\":\"${THE_WASM_INTERPRETER_SHA256}\",\"entry\":\"llcbrowser.run\",\"image_crexx\":\"${THE_CREXX_INTERPRETER_IMAGE_REVISION}\"}\n")
add_custom_target(the_wasm_runner ALL
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:crxthe_rxvm>"
        "${CMAKE_BINARY_DIR}/crxthe_rxvm.wasm" "${runner_site}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/web/vm/vm-worker.mjs"
        "${CMAKE_SOURCE_DIR}/web/vm/byte-input.mjs" "${runner_site}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${THE_WASM_INTERPRETER_IMAGE}" "${runner_site}/llcbrowser.rxbin"
    DEPENDS crxthe_rxvm "${THE_WASM_INTERPRETER_IMAGE}" VERBATIM)
