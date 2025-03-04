cmake_minimum_required(VERSION 3.14...3.31)
project(pngcheck
        VERSION 4.0.0
        DESCRIPTION "PNG file checker"
        HOMEPAGE_URL "http://www.libpng.org/pub/png/apps/pngcheck.html"
        LANGUAGES C
)

# Options for zlib support
option(PNGCHECK_USE_ZLIB "Enable zlib support" ON)
option(PNGCHECK_USE_SYSTEM_ZLIB "Use the system-installed zlib" ON)

# Options for supplemental builds
option(PNGCHECK_ENABLE_AUX_TOOLS "Build and install the auxiliary tools" OFF)
if(PNGCHECK_ENABLE_AUX_TOOLS AND NOT PNGCHECK_USE_ZLIB)
    message(FATAL_ERROR "The auxiliary tools require zlib support")
endif()

# Source files
set(PNGCHECK_SOURCES pngcheck.c)
if(PNGCHECK_ENABLE_AUX_TOOLS)
    set(PNGSPLIT_SOURCES gpl/pngsplit.c)
    set(PNG_FIX_IDAT_WINDOWSIZE_SOURCES gpl/png-fix-IDAT-windowsize.c)
endif()

# Executables
add_executable(pngcheck ${PNGCHECK_SOURCES})
if(PNGCHECK_ENABLE_AUX_TOOLS)
    add_executable(pngsplit ${PNGSPLIT_SOURCES})
    add_executable(png-fix-IDAT-windowsize ${PNG_FIX_IDAT_WINDOWSIZE_SOURCES})
endif()

# Dependency handling
if(PNGCHECK_USE_ZLIB)
    if(PNGCHECK_USE_SYSTEM_ZLIB)
        find_package(ZLIB)
        if(ZLIB_FOUND)
            target_link_libraries(pngcheck PRIVATE ZLIB::ZLIB)
            if(PNGCHECK_ENABLE_AUX_TOOLS)
                target_link_libraries(pngsplit PRIVATE ZLIB::ZLIB)
                target_link_libraries(png-fix-IDAT-windowsize PRIVATE ZLIB::ZLIB)
            endif()
        else()
            message(WARNING "System zlib not found, falling back to FetchContent")
            set(PNGCHECK_USE_SYSTEM_ZLIB OFF)
        endif()
    endif()
    if(NOT PNGCHECK_USE_SYSTEM_ZLIB)
        include(FetchContent)
        FetchContent_Declare(zlib
                             GIT_REPOSITORY https://github.com/madler/zlib.git
                             GIT_TAG v1.3.1
        )
        FetchContent_MakeAvailable(zlib)
        target_link_libraries(pngcheck PRIVATE zlibstatic)
        if(PNGCHECK_ENABLE_AUX_TOOLS)
            target_link_libraries(pngsplit PRIVATE zlibstatic)
            target_link_libraries(png-fix-IDAT-windowsize PRIVATE zlibstatic)
        endif()
    endif()
    target_compile_definitions(pngcheck PRIVATE USE_ZLIB)
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS pngcheck
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
)
install(FILES pngcheck.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
        COMPONENT Documentation
)
if(PNGCHECK_ENABLE_AUX_TOOLS)
    install(TARGETS pngsplit png-fix-IDAT-windowsize
            RUNTIME
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT Runtime
    )
    install(FILES gpl/pngsplit.1 gpl/png-fix-IDAT-windowsize.1
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
            COMPONENT Documentation
    )
endif()

# Package generation
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
include(CPack)
