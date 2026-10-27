if(NOT EMSCRIPTEN OR NOT EMSCRIPTEN_VERSION VERSION_EQUAL "6.0.13")
    message(FATAL_ERROR "Use Emscripten 6.0.13 for the browser VM")
endif()
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
include("${CMAKE_SOURCE_DIR}/cmake/CrexxWasm.cmake")
the_add_wasm_vm()
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
