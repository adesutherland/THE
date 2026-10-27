if(NOT EMSCRIPTEN OR NOT EMSCRIPTEN_VERSION VERSION_EQUAL "6.0.13")
    message(FATAL_ERROR "Use Emscripten 6.0.13 for the browser VM")
endif()
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
include("${CMAKE_SOURCE_DIR}/cmake/CrexxWasm.cmake")
add_executable(crxthe_rxvm src/wasm/crexxvm.c src/wasm/vmstreams.c src/wasm/crexxresources.c)
target_link_libraries(crxthe_rxvm PRIVATE rxvm_single_embed)
set_target_properties(crxthe_rxvm PROPERTIES SUFFIX ".mjs")
target_link_options(crxthe_rxvm PRIVATE --no-entry
    -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=1048576 -sSTACK_SIZE=1048576
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sASSERTIONS=1
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=worker,node
    "-sEXPORTED_FUNCTIONS=['_the_vm_create','_the_vm_destroy','_the_vm_load','_the_vm_run','_the_vm_program_rc','_the_vm_error','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','UTF8ToString','HEAPU8','HEAPU32']"
    -Wl,--wrap=__wasi_fd_read,--wrap=__wasi_fd_write)
find_program(THE_VM_RXC NAMES rxc HINTS "${THE_CREXX_HOST_BUILD}/bin" NO_DEFAULT_PATH REQUIRED)
find_program(THE_VM_RXAS NAMES rxas HINTS "${THE_CREXX_HOST_BUILD}/bin" NO_DEFAULT_PATH REQUIRED)
set(THE_VM_SITE "${CMAKE_BINARY_DIR}/site")
find_program(THE_VM_PYTHON NAMES python3 REQUIRED)
file(MAKE_DIRECTORY "${THE_VM_SITE}/modules")
add_custom_command(OUTPUT "${THE_VM_SITE}/modules/stdio-probe.rxbin"
    COMMAND "${THE_VM_RXC}" -x --no-exe-import -i "${THE_CREXX_HOST_BUILD}/bin"
        -o "${CMAKE_BINARY_DIR}/stdio-probe" "${CMAKE_SOURCE_DIR}/tests/wasm-vm-stdio.crexx"
    COMMAND "${THE_VM_RXAS}" -o "${THE_VM_SITE}/modules/stdio-probe"
        "${CMAKE_BINARY_DIR}/stdio-probe"
    DEPENDS tests/wasm-vm-stdio.crexx "${THE_VM_RXC}" "${THE_VM_RXAS}"
        "${THE_CREXX_HOST_BUILD}/bin/library.rxbin"
    VERBATIM)
add_custom_target(the_wasm_vm_bundle ALL
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:crxthe_rxvm>"
        "${CMAKE_BINARY_DIR}/crxthe_rxvm.wasm" "${THE_VM_SITE}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/web/vm" "${THE_VM_SITE}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${THE_CREXX_HOST_BUILD}/bin/library.rxbin"
        "${THE_VM_SITE}/modules/library.rxbin"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/web/test/wasm-vm-contract.html"
        "${THE_VM_SITE}/test.html"
    COMMAND "${THE_VM_PYTHON}" "${CMAKE_SOURCE_DIR}/scripts/package-wasm-site.py"
        "${THE_VM_SITE}" "${THE_VERSION}" "${EMSCRIPTEN_VERSION}"
        --emscripten-license "${EMSCRIPTEN_ROOT_PATH}/LICENSE"
        --crexx-source "${THE_CREXX_SOURCE}" --crexx-revision "${THE_CREXX_REVISION}" --vm-only
    DEPENDS crxthe_rxvm "${THE_VM_SITE}/modules/stdio-probe.rxbin"
    VERBATIM)
