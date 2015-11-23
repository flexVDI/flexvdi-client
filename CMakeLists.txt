cmake_minimum_required(VERSION 2.8)

# Project information
project(flexvdi-spice-client)
set(FLEXVDI_VERSION_MAJOR 2)
set(FLEXVDI_VERSION_MINOR 2)
set(FLEXVDI_VERSION_PATCH 1)
set(FLEXVDI_VERSION ${FLEXVDI_VERSION_MAJOR}.${FLEXVDI_VERSION_MINOR}.${FLEXVDI_VERSION_PATCH})
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
message("${CMAKE_PROJECT_NAME} v${FLEXVDI_VERSION} ${CMAKE_BUILD_TYPE} build")


## Find required packages
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")
include(FindPkgConfig)
include(CheckFunctionExists)
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
if (BIG_ENDIAN)
    add_definitions(-DWORDS_BIGENDIAN)
endif ()
macro(check_client_module modname pkgname)
    pkg_check_modules(${modname} ${pkgname})
    if (${modname}_FOUND)
        include_directories(${${modname}_INCLUDE_DIRS})
        link_directories(${${modname}_LIBRARY_DIRS})
        set(CLIENT_LIBRARIES ${CLIENT_LIBRARIES} ${${modname}_LIBRARIES})
        set(${modname}_VERSION_STR v${${modname}_VERSION})
        set(REQUIRED_PKGS "${REQUIRED_PKGS}, ${pkgname}")
    else ()
        set(${modname}_VERSION_STR No)
    endif ()
endmacro ()

check_function_exists(strnlen HAVE_STRNLEN)
if (HAVE_STRNLEN)
    add_definitions(-DHAVE_STRNLEN)
endif ()
check_client_module(flexdp "flexdp >= 2.2.1")
check_client_module(spice_glib spice-client-glib-2.0)

if (WIN32)
    check_client_module(CAIRO cairo)
    check_client_module(POPPLER poppler-glib)
    if (CAIRO_FOUND AND POPPLER_FOUND)
        set(WITH_PRINTING Yes)
    endif ()
else ()
    check_client_module(gio_unix gio-unix-2.0)
    find_package(Cups)
    if (CUPS_FOUND)
        include_directories(${CUPS_INCLUDE_DIR})
        set(CLIENT_LIBRARIES ${CLIENT_LIBRARIES} ${CUPS_LIBRARIES})
        set(WITH_PRINTING Yes)
        set(CUPS_VERSION_STR v${CUPS_VERSION_STRING})
        set(EXTRA_PKGS_LDFLAGS "${EXTRA_PKGS_LDFLAGS} ${CUPS_LIBRARIES}")
        set(EXTRA_PKGS_CFLAGS "${EXTRA_PKGS_CFLAGS} -I${CUPS_INCLUDE_DIR}")
    else ()
        set(CUPS_VERSION_STR No)
    endif ()
    if (gio_unix_FOUND)
        set(WITH_SERIALREDIR Yes)
    endif ()
endif ()
if (NOT flexdp_FOUND OR NOT spice_glib_FOUND)
    message(FATAL_ERROR "flexDP and Spice client are needed to build the client library.")
endif ()
string(SUBSTRING "${REQUIRED_PKGS}" 2 -1 REQUIRED_PKGS)

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


# Summary
message("\nConfigure summary\n=================\n")
message("  flexVDI Desktop Protocol: ${flexdp_VERSION_STR}")
message("  Spice client:             ${spice_glib_VERSION_STR}")
if (WIN32)
message("  Cairo (Print):            ${CAIRO_VERSION_STR}")
message("  Poppler (Print):          ${POPPLER_VERSION_STR}")
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
         CLIENT_LIBRARY_DIRS CLIENT_INCLUDE_DIRS CLIENT_LIBRARIES
         )
    if (${varname})
        message("${varname} = ${${varname}}")
    endif ()
endforeach ()
message("")

# Build client library
add_subdirectory(client)

# Build tests
add_subdirectory(test)

# Packages
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${FLEXVDI_VERSION}")
add_custom_target(dist
                  COMMAND git archive --prefix=${CPACK_PACKAGE_FILE_NAME}/
                          -o "${PROJECT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.tar.bz2" HEAD
                  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
