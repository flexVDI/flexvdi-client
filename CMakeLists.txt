cmake_minimum_required(VERSION 2.8)

# Project information
project(flexvdi-client)
set(FLEXVDI_VERSION_MAJOR 3)
set(FLEXVDI_VERSION_MINOR 1)
set(FLEXVDI_VERSION_PATCH 2)
set(FLEXVDI_VERSION ${FLEXVDI_VERSION_MAJOR}.${FLEXVDI_VERSION_MINOR}.${FLEXVDI_VERSION_PATCH})
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
message("${CMAKE_PROJECT_NAME} v${FLEXVDI_VERSION} ${CMAKE_BUILD_TYPE} build")
if (CMAKE_C_FLAGS MATCHES ".*-DANDROID.*")
    set(ANDROID 1)
endif ()


## Find required packages
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")
include(FindPkgConfig)
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
if (BIG_ENDIAN)
    add_definitions(-DWORDS_BIGENDIAN)
endif ()
macro(check_module target modname pkgname)
    pkg_check_modules(${modname} ${pkgname})
    if (${modname}_FOUND)
        include_directories(${${modname}_INCLUDE_DIRS})
        link_directories(${${modname}_LIBRARY_DIRS})
        set(${target}_LIBRARIES ${${target}_LIBRARIES} ${${modname}_LIBRARIES})
        set(${modname}_VERSION_STR v${${modname}_VERSION})
    else ()
        message(FATAL_ERROR "${pkgname} is needed to build the flexVDI Client.")
    endif ()
endmacro ()

check_module(LIB flexdp "flexdp >= 2.2.4")
check_module(LIB spice_glib spice-client-glib-2.0)
check_module(LIB glib glib-2.0)
check_module(LIB gobject gobject-2.0)
check_module(LIB gio gio-2.0)
check_module(LIB libsoup libsoup-2.4)
check_module(LIB json json-glib-1.0)
check_module(CLIENT gst gstreamer-1.0)

if (WIN32)
    check_module(LIB CAIRO cairo)
    check_module(LIB POPPLER poppler-glib)
    check_module(CLIENT spice_gtk spice-client-gtk-3.0)
elseif (ANDROID OR IOS)
    # No extra dependency on mobile devices
else ()
    if (NOT APPLE)
        add_definitions(-DENABLE_SERIALREDIR)
        check_module(LIB gio_unix gio-unix-2.0)
    endif ()
    find_package(Cups 2.0)
    if (CUPS_FOUND)
        include_directories(${CUPS_INCLUDE_DIR})
        set(LIB_LIBRARIES ${LIB_LIBRARIES} ${CUPS_LIBRARIES})
        set(CUPS_VERSION_STR v${CUPS_VERSION_STRING})
        set(EXTRA_PKGS_LDFLAGS "${EXTRA_PKGS_LDFLAGS} ${CUPS_LIBRARIES}")
        set(EXTRA_PKGS_CFLAGS "${EXTRA_PKGS_CFLAGS} -I${CUPS_INCLUDE_DIR}")
    else ()
        message(FATAL_ERROR "Cups is needed to build the flexVDI Client.")
    endif ()
    check_module(CLIENT spice_gtk spice-client-gtk-3.0)
endif ()

set(CLIENT_LIBRARIES ${CLIENT_LIBRARIES} ${LIB_LIBRARIES})

## Build options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
if (CMAKE_BUILD_TYPE STREQUAL Release)
    set(CMAKE_EXE_LINKER_FLAGS "-s")
endif ()
if (WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
    # WINVER = 0x0501 -> Windows XP
    add_definitions(-DWIN32 -DUNICODE -D_UNICODE -DWINVER=0x0501 -DWIN32_LEAN_AND_MEAN)
endif ()
add_definitions(-DG_LOG_USE_STRUCTURED)


# Summary
message("\nConfigure summary\n=================\n")
message("  flexVDI Desktop Protocol: ${flexdp_VERSION_STR}")
message("  Spice client:             ${spice_glib_VERSION_STR}")
message("  libsoup:                  ${libsoup_VERSION_STR}")
message("  JSON-GLib:                ${json_VERSION_STR}")
if (WIN32)
message("  Cairo (Print):            ${CAIRO_VERSION_STR}")
message("  Poppler (Print):          ${POPPLER_VERSION_STR}")
elseif (ANDROID OR IOS)
# Pass
elseif (APPLE)
message("  Cups (Print):             ${CUPS_VERSION_STR}")
else ()
message("  gio-unix (Serial redir.): ${gio_unix_VERSION_STR}")
message("  Cups (Print):             ${CUPS_VERSION_STR}")
endif ()
message("\nUseful variables:")
foreach (varname
         CMAKE_C_FLAGS CMAKE_BUILD_TYPE
         flexdp_INCLUDE_DIRS CUPS_INCLUDE_DIR
         CAIRO_LIBRARY_DIRS CAIRO_INCLUDE_DIRS
         POPPLER_LIBRARY_DIRS POPPLER_INCLUDE_DIRS
         gio_unix_LIBRARY_DIRS gio_unix_INCLUDE_DIRS
         libsoup_LIBRARY_DIRS libsoup_INCLUDE_DIRS
         json_LIBRARY_DIRS json_INCLUDE_DIRS
         CLIENT_LIBRARY_DIRS CLIENT_INCLUDE_DIRS CLIENT_LIBRARIES
         )
    if (${varname})
        message("${varname} = ${${varname}}")
    endif ()
endforeach ()
message("")

# Build resources
if (NOT ANDROID AND NOT IOS)
add_subdirectory(resources)
endif ()

# Build client library
add_subdirectory(src)

# Build tests
if (NOT ANDROID AND NOT IOS)
enable_testing()
add_subdirectory(test)
endif ()

# Packages
include(CheckCSourceCompiles)
check_c_source_compiles(
    "#if !defined(__i386) && !defined(__i386__) && !defined(_M_IX86)
     #error This is not x86
     #endif
     int main() { return 0; }"
    ARCH_IS_I386
)
if (ARCH_IS_I386)
    set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${FLEXVDI_VERSION}-i686")
else ()
    set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${FLEXVDI_VERSION}-x86_64")
endif ()
add_custom_target(dist
                  COMMAND git archive --prefix=${CPACK_PACKAGE_FILE_NAME}/
                          -o "${PROJECT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.tar.bz2" HEAD
                  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
if (WIN32)
    if (ARCH_IS_I386)
        set(PROGRAMFILES "$PROGRAMFILES")
    else ()
        set(PROGRAMFILES "$PROGRAMFILES64")
    endif ()
    configure_file("${PROJECT_SOURCE_DIR}/scripts/setup-tpl.nsi"
                   "${PROJECT_BINARY_DIR}/setup.nsi" @ONLY)
    add_custom_target(installer DEPENDS flexvdi-client-bin "${PROJECT_BINARY_DIR}/src/icon.ico"
                      COMMAND "${PROJECT_SOURCE_DIR}/scripts/build_installer_win.sh" ${CMAKE_INSTALL_PREFIX} ${CMAKE_BUILD_TYPE}
                      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
elseif (IOS)
# No installer
elseif (APPLE)
    configure_file("${PROJECT_SOURCE_DIR}/scripts/Info-tpl.plist"
                   "${PROJECT_BINARY_DIR}/Info.plist" @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/scripts/flexvdi-client.bundle"
                   "${PROJECT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.bundle" COPYONLY)
    add_custom_target(installer DEPENDS flexvdi-client-bin
                      COMMAND "${PROJECT_SOURCE_DIR}/scripts/build_dmg.sh" ${CMAKE_INSTALL_PREFIX} ${CPACK_PACKAGE_FILE_NAME}
                      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
elseif (ANDROID)
    add_custom_target(installer DEPENDS flexvdi-client-static flexvdi-client)
else ()
    add_custom_target(installer DEPENDS flexvdi-client-bin
                      COMMAND "${PROJECT_SOURCE_DIR}/scripts/build_appimage.sh" ${CMAKE_INSTALL_PREFIX} ${CPACK_PACKAGE_FILE_NAME} ${CMAKE_BUILD_TYPE}
                      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif ()
