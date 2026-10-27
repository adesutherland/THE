include_guard(GLOBAL)
set(THE_CREXX_REVISION "c566025eedc5ef9f48b4c4a8f8f8919fd89793c5" CACHE STRING "Pinned cREXX VM/tool source revision")
set(THE_CREXX_SOURCE "${CMAKE_SOURCE_DIR}/../CREXX" CACHE PATH "Matching cREXX source checkout")
set(THE_CREXX_HOST_BUILD "${CMAKE_SOURCE_DIR}/build/crexx-host" CACHE PATH "Matching native cREXX build")
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${THE_CREXX_SOURCE}"
    OUTPUT_VARIABLE crexx_revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT crexx_revision STREQUAL THE_CREXX_REVISION)
    message(FATAL_ERROR "The Wasm VM requires cREXX ${THE_CREXX_REVISION}; use a pinned dependency checkout")
endif()

function(the_add_wasm_vm)
    add_executable(crxthe_rxvm ${CMAKE_SOURCE_DIR}/src/wasm/crexxvm.c
        ${CMAKE_SOURCE_DIR}/src/wasm/vmstreams.c ${CMAKE_SOURCE_DIR}/src/wasm/crexxresources.c)
    target_link_libraries(crxthe_rxvm PRIVATE rxvm_single_embed)
    set_target_properties(crxthe_rxvm PROPERTIES SUFFIX ".mjs")
    target_link_options(crxthe_rxvm PRIVATE --no-entry
        -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=1048576 -sSTACK_SIZE=1048576
        -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sASSERTIONS=1
        -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=worker,node
        "-sEXPORTED_FUNCTIONS=['_the_vm_create','_the_vm_destroy','_the_vm_load','_the_vm_run','_the_vm_program_rc','_the_vm_error','_malloc','_free']"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','UTF8ToString','HEAPU8','HEAPU32']"
        -Wl,--wrap=__wasi_fd_read,--wrap=__wasi_fd_write)
endfunction()
# Unrelated frontend work may coexist in a developer checkout. The VM and
# native tools used by this target must still be the pinned source cohort.
execute_process(COMMAND "${GIT_EXECUTABLE}" diff --quiet HEAD --
    CMakeLists.txt cmake interpreter platform binutils assembler compiler lib rxpa third_party ports/single-threaded
    WORKING_DIRECTORY "${THE_CREXX_SOURCE}" RESULT_VARIABLE crexx_modified)
if(NOT crexx_modified EQUAL 0)
    message(FATAL_ERROR "cREXX VM/tool sources have uncommitted changes; use the pinned checkout")
endif()
set(CREXX_GENERATED_DIR "${THE_CREXX_HOST_BUILD}/generated" CACHE PATH "Native cREXX generated headers")
file(READ "${CREXX_GENERATED_DIR}/crexx_version.h" crexx_version_header)
if(NOT crexx_version_header MATCHES "CREXX_BUILD_COMMIT \"${THE_CREXX_REVISION}\"")
    message(FATAL_ERROR "Native cREXX tools and generated headers must match ${THE_CREXX_REVISION}")
endif()
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CREXX_BUILD_COMPILER OFF CACHE BOOL "" FORCE)
set(CREXX_VM_CLASSIC_PROVIDERS OFF CACHE BOOL "" FORCE)
set(CREXX_MAINFRAME_ADDRESS OFF CACHE BOOL "" FORCE)
add_compile_definitions(_GNU_SOURCE=1)
add_subdirectory("${THE_CREXX_SOURCE}/ports/single-threaded" crexx-vm EXCLUDE_FROM_ALL)
# This embedding leaf predates the shared invocation owner. Keep it linked
# without modifying the dependency checkout; omit this addition once upstream
# includes it in the maintained leaf.
get_target_property(vm_sources rxvm_single_core SOURCES)
if(NOT "${vm_sources}" MATCHES "/rxvminvoke\\.c")
    target_sources(rxvm_single_core PRIVATE "${THE_CREXX_SOURCE}/interpreter/rxvminvoke.c")
endif()
