message(STATUS "**** ${PROJECT_NAME} ****")

set(PDCURSES_SRCDIR ${pdcurses_SOURCE_DIR})
set(PDCURSES_DIST ${CMAKE_INSTALL_PREFIX}/${CMAKE_BUILD_TYPE})

set(osdir ${PDCURSES_SRCDIR}/${PROJECT_NAME})

set(pdc_src_files
    ${osdir}/pdcclip.c
    ${osdir}/pdcdisp.c
    ${osdir}/pdcgetsc.c
    ${osdir}/pdckbd.c
    ${osdir}/pdcscrn.c
    ${osdir}/pdcsetsc.c
    ${osdir}/pdcutil.c
)

include_directories (..)
include_directories (${osdir})


if(WIN32 AND NOT WATCOM)
    include(dll_version)
    list(APPEND pdc_src_files ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

    add_definitions(-D_WIN32 -D_CRT_SECURE_NO_WARNINGS)

    if(${TARGET_ARCH} STREQUAL "ARM" OR ${TARGET_ARCH} STREQUAL "ARM64")
        add_definitions(-D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1)
    endif()

    set(EXTRA_LIBS gdi32.lib winspool.lib shell32.lib ole32.lib comdlg32.lib advapi32.lib)
    set(WINCON_DEP_LIBS winmm.lib)
elseif(WATCOM_WIN32)
    set(EXTRA_LIBS "")
    set(WINCON_DEP_LIBS winmm.lib)
else()
    set(EXTRA_LIBS "")
    set(WINCON_DEP_LIBS "")
endif()

if(PDC_BUILD_SHARED)
    set(PDCURSE_PROJ ${PROJECT_NAME}_pdcurses)
    add_library(${PDCURSE_PROJ} SHARED ${pdc_src_files} ${pdcurses_src_files})

    target_link_libraries(${PDCURSE_PROJ} ${EXTRA_LIBS} ${WINCON_DEP_LIBS})

    install(TARGETS ${PDCURSE_PROJ}
        ARCHIVE DESTINATION ${PDCURSES_DIST}/lib/${PROJECT_NAME}
        LIBRARY DESTINATION ${PDCURSES_DIST}/lib/${PROJECT_NAME}
        RUNTIME DESTINATION ${PDCURSES_DIST}/bin/${PROJECT_NAME} COMPONENT applications)
    set_target_properties(${PDCURSE_PROJ} PROPERTIES OUTPUT_NAME "pdcurses")
else()
    set(PDCURSE_PROJ ${PROJECT_NAME}_pdcursesstatic)
    add_library (${PDCURSE_PROJ} STATIC ${pdc_src_files} ${pdcurses_src_files})
    install (TARGETS ${PDCURSE_PROJ} ARCHIVE DESTINATION ${PDCURSES_DIST}/lib/${PROJECT_NAME} COMPONENT applications)
    set_target_properties(${PDCURSE_PROJ} PROPERTIES OUTPUT_NAME "pdcursesstatic")
endif()
