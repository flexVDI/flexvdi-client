cmake_minimum_required(VERSION 2.8)

# Project information
project(flexvdi-guest-tools)
set(FLEXVDI_VERSION_MAJOR 2)
set(FLEXVDI_VERSION_MINOR 2)
set(FLEXVDI_VERSION_PATCH 0)
set(FLEXVDI_VERSION ${FLEXVDI_VERSION_MAJOR}.${FLEXVDI_VERSION_MINOR}.${FLEXVDI_VERSION_PATCH})
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
message("${CMAKE_PROJECT_NAME} v${FLEXVDI_VERSION} ${CMAKE_BUILD_TYPE} build")


## Find required packages
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")
include(FindPkgConfig)
pkg_check_modules(spice_protocol REQUIRED spice-protocol)
include_directories(${spice_protocol_INCLUDE_DIRS})
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)


## Find client packages
if (NOT DEFINED BUILD_CLIENT)
    set(BUILD_CLIENT Yes)
endif ()
macro(check_client_module modname pkgname)
    pkg_check_modules(${modname} ${pkgname})
    if (NOT ${modname}_FOUND)
        set(${modname}_FOUND_STR No)
    else ()
        set(CLIENT_LIBRARY_DIRS ${CLIENT_LIBRARY_DIRS} ${${modname}_LIBRARY_DIRS})
        set(CLIENT_INCLUDE_DIRS ${CLIENT_INCLUDE_DIRS} ${${modname}_INCLUDE_DIRS})
        set(CLIENT_LIBRARIES    ${CLIENT_LIBRARIES}    ${${modname}_LIBRARIES})
        set(${modname}_FOUND_STR "Yes v${${modname}_VERSION}")
    endif ()
endmacro ()

# std::thread support in libstdc++
set (CMAKE_REQUIRED_LIBRARIES pthread)
set (CMAKE_REQUIRED_FLAGS -std=c++11)
include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
    "#include <mutex>
     #include <thread>
     int main() { std::mutex m; std::thread t; return 0; }"
    HAVE_STD_THREAD
)

# strnlen
include(CheckFunctionExists)
check_function_exists(strnlen HAVE_STRNLEN)

# Spice client glib
check_client_module(spice_glib spice-client-glib-2.0)

# Cairo
if (WIN32)
    check_client_module(CAIRO cairo)
endif ()

# Poppler
if (WIN32)
    check_client_module(POPPLER poppler-glib)
endif ()

# gio-unix
if (NOT WIN32)
    check_client_module(gio_unix gio-unix-2.0)
endif ()

# CUPS
if (NOT WIN32)
    find_package(Cups)
    if (NOT CUPS_FOUND)
        set(CUPS_FOUND_STR No)
    else ()
        set(CLIENT_INCLUDE_DIRS ${CLIENT_INCLUDE_DIRS} ${CUPS_INCLUDE_DIR})
        set(CLIENT_LIBRARIES    ${CLIENT_LIBRARIES}    ${CUPS_LIBRARIES})
        set(CUPS_FOUND_STR "Yes v${CUPS_VERSION_STRING}")
    endif ()
endif ()

# Client required modules
if (WIN32)
    if (NOT spice_glib_FOUND OR NOT CAIRO_FOUND OR NOT POPPLER_FOUND)
        message("Spice client, Cairo and Poppler are needed to build the client library, skipping it.")
        set(BUILD_CLIENT No)
    endif ()
else ()
    if (NOT spice_glib_FOUND OR NOT gio_unix_FOUND OR NOT CUPS_FOUND)
        message("Spice client, Cups and GIO-unix are needed to build the client library, skipping it.")
        set(BUILD_CLIENT No)
    endif ()
endif ()
include_directories(${CLIENT_INCLUDE_DIRS})
link_directories(${CLIENT_LIBRARY_DIRS})


## Find guest packages
if (NOT DEFINED BUILD_GUEST)
    set(BUILD_GUEST Yes)
endif ()

# Boost
if (NOT WIN32)
    set(Boost_USE_STATIC_LIBS ON)
endif ()
if (NOT HAVE_STD_THREAD)
    set(Boost_USE_THREAD thread)
endif ()
find_package(Boost 1.49.0 COMPONENTS system ${Boost_USE_THREAD} unit_test_framework)
if (NOT Boost_FOUND)
    message("Boost >1.49 is needed to build the guest agent, skipping it")
    set(BUILD_GUEST No)
    set(Boost_FOUND_STR No)
else ()
    set(FLEXVDI_LIBRARIES ${FLEXVDI_LIBRARIES} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY})
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
    set(Boost_FOUND_STR "Yes v${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")
endif ()

# Cups
if (NOT WIN32)
    if (NOT CUPS_FOUND)
        message("Cups is needed to build the guest agent, skipping it")
        set(BUILD_GUEST No)
    else ()
        set(FLEXVDI_LIBRARIES ${FLEXVDI_LIBRARIES} ${CUPS_LIBRARIES})
    endif ()
endif ()

# SELinux policy compiler
if (NOT WIN32)
    find_file(SELINUX_MAKEFILE Makefile
                 /usr/share/selinux/devel
                 /usr/share/selinux/default/include
                 /usr/share/selinux/strict/include
                 /etc/security/selinux/src/policy)
endif ()


# Build options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
if (CMAKE_BUILD_TYPE STREQUAL Release)
    set(CMAKE_EXE_LINKER_FLAGS "-s")
endif (CMAKE_BUILD_TYPE STREQUAL Release)
set (FLEXVDI_LIBRARIES ${FLEXVDI_LIBRARIES} pthread)
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
    # WINVER = 0x0501 -> Windows XP
    add_definitions(-DWIN32 -DUNICODE -D_UNICODE -DWINVER=0x0501 -DWIN32_LEAN_AND_MEAN)
    set (FLEXVDI_LIBRARIES ${FLEXVDI_LIBRARIES} wsock32 setupapi)
    string(REPLACE "." "," FLEXVDI_VERSION_COMA "${FLEXVDI_VERSION}")
    # FIXME: This is a workaround to a multiple definition of symbols in static libstdc++,
    # MinGW-w64 GCC 5.1.0 Fedora 22
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-multiple-definition")
endif ()


# Summary
message("\nConfigure summary\n=================\n")
message("Spice Protocol:         v${spice_protocol_VERSION}")
message("Build client library:   ${BUILD_CLIENT}")
message("  Spice client:         ${spice_glib_FOUND_STR}")
if (WIN32)
message("  Cairo (Print):        ${CAIRO_FOUND_STR}")
message("  Poppler (Print):      ${POPPLER_FOUND_STR}")
else ()
message("  gio-unix (Serial):    ${gio_unix_FOUND_STR}")
message("  Cups (Print):         ${CUPS_FOUND_STR}")
endif ()
message("Build guest tools:      ${BUILD_GUEST}")
message("  Boost:                ${Boost_FOUND_STR}")
message("\nUseful variables:")
foreach (varname
         CMAKE_CXX_FLAGS CMAKE_C_FLAGS CMAKE_BUILD_TYPE
         Boost_INCLUDE_DIRS Boost_LIBRARY_DIRS CUPS_INCLUDE_DIR FLEXVDI_LIBRARIES
         CAIRO_LIBRARY_DIRS CAIRO_INCLUDE_DIRS
         POPPLER_LIBRARY_DIRS POPPLER_INCLUDE_DIRS
         gio_unix_LIBRARY_DIRS gio_unix_INCLUDE_DIRS
         CLIENT_LIBRARY_DIRS CLIENT_INCLUDE_DIRS CLIENT_LIBRARIES
         SELINUX_MAKEFILE)
    if (${varname})
        message("${varname} = ${${varname}}")
    endif ()
endforeach ()
message("")

if (NOT BUILD_CLIENT AND NOT BUILD_GUEST)
    message(FATAL_ERROR "Not building client nor guest")
endif ()


# Sources and objects
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories(${PROJECT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
set(agent_sources FlexVDIGuestAgent.cpp DispatcherRegistry.cpp VirtioPort.cpp LocalPipe.cpp)
set(common_sources util.cpp Connection.cpp LocalPipeClient.cpp WinSpoolImpl.cpp)

# Build guest agent
if (BUILD_GUEST)
    set(GUEST_TOOLS_TARGETS)
    add_subdirectory(credentials-manager)
    add_subdirectory(print)

    add_library(agent_objects OBJECT ${agent_sources})
    add_library(common_objects OBJECT ${common_sources})
    if (WIN32)
        configure_file("${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.rc"
                       "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc" @ONLY)
        add_executable(flexvdi-guest-agent WindowsService.cpp
                       "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc"
                       $<TARGET_OBJECTS:agent_objects> $<TARGET_OBJECTS:common_objects>)
        set_target_properties(flexvdi-guest-agent PROPERTIES LINK_FLAGS "-static")
        target_link_libraries(flexvdi-guest-agent ${FLEXVDI_LIBRARIES})
        configure_file("${PROJECT_SOURCE_DIR}/setup.nsi.in"
                       "${PROJECT_BINARY_DIR}/setup.nsi" @ONLY)
    else (WIN32)
        add_executable(flexvdi-guest-agent main.cpp
                       $<TARGET_OBJECTS:agent_objects> $<TARGET_OBJECTS:common_objects>)
        target_link_libraries(flexvdi-guest-agent ${FLEXVDI_LIBRARIES})
        configure_file("${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.sh.in"
                       "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.sh" @ONLY)
        configure_file("${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.service.in"
                       "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.service" @ONLY)
        configure_file("${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.te.in"
                       "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.te")

        install(TARGETS flexvdi-guest-agent RUNTIME DESTINATION bin)
        install(FILES "${PROJECT_BINARY_DIR}/flexvdi-guest-agent"
                DESTINATION libexec/flexvdi-guest-agent
                PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
        install(FILES "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.service"
                DESTINATION lib/systemd/system)
        if (EXISTS "${SELINUX_MAKEFILE}")
            add_custom_command(OUTPUT flexvdi-guest-agent.pp
                               COMMAND make -f "${SELINUX_MAKEFILE}" flexvdi-guest-agent.pp
                               MAIN_DEPENDENCY "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.te"
                               WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
            add_custom_target(flexvdi-selinux-policy ALL DEPENDS flexvdi-guest-agent.pp)
            install(FILES "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.pp"
                    DESTINATION share/selinux/targeted)
        endif ()
    endif (WIN32)
    set(GUEST_TOOLS_TARGETS ${GUEST_TOOLS_TARGETS} flexvdi-guest-agent)
endif ()

# Build client library
if (BUILD_CLIENT)
    add_subdirectory(client)
endif ()

# Build tests
add_subdirectory(test)

# Packages
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${FLEXVDI_VERSION}")
add_custom_target(dist
                  COMMAND git archive --prefix=${CPACK_PACKAGE_FILE_NAME}/
                          -o "${PROJECT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.tar.bz2" HEAD
                  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
if (BUILD_GUEST)
    if (WIN32)
        configure_file("${PROJECT_SOURCE_DIR}/autorun.inf.in"
                       "${PROJECT_BINARY_DIR}/autorun.inf")
        add_custom_command(OUTPUT virtio-win.iso
                           COMMAND wget -nc https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/latest-virtio/virtio-win.iso)
        add_custom_target(virtio_win_iso DEPENDS virtio-win.iso)
        add_custom_command(OUTPUT spice-nsis
                           COMMAND git clone git://anongit.freedesktop.org/spice/spice-nsis)
        add_custom_target(spice_nsis_repo DEPENDS spice-nsis)
        add_custom_command(OUTPUT spice-guest-tools.exe
                           COMMAND "${PROJECT_SOURCE_DIR}/configure-spice-nsis.sh" ${WIN_COMPONENTS_ANCESTORS}
                           COMMAND echo Running makensis, see spice-nsis.log for errors
                           COMMAND makensis -Ospice-nsis.log -X'!AddPluginDir spice-nsis' spice-nsis/spice-guest-tools.nsis
                           COMMAND mv spice-nsis/spice-guest-tools*.exe spice-guest-tools.exe
                           DEPENDS virtio_win_iso spice_nsis_repo)
        add_custom_target(spice_guest_tools_installer DEPENDS spice-guest-tools.exe)
        add_custom_command(OUTPUT ${CPACK_PACKAGE_FILE_NAME}.exe
                           COMMAND "${PROJECT_SOURCE_DIR}/find_win_components.sh" ${WIN_COMPONENTS_ANCESTORS}
                           COMMAND echo Running makensis, see makensis.log for errors
                           COMMAND makensis -Omakensis.log setup.nsi
                           DEPENDS ${GUEST_TOOLS_TARGETS} ${PROJECT_BINARY_DIR}/setup.nsi spice_guest_tools_installer)
        add_custom_target(installer DEPENDS ${CPACK_PACKAGE_FILE_NAME}.exe)
        add_custom_command(OUTPUT ${CPACK_PACKAGE_FILE_NAME}.iso
                           COMMAND cp virtio-win.iso ${CPACK_PACKAGE_FILE_NAME}.iso
                           COMMAND growisofs -M ${CPACK_PACKAGE_FILE_NAME}.iso
                                           -J -r -V "flexVDI Guest Tools v${FLEXVDI_VERSION}"
                                           autorun.inf ${CPACK_PACKAGE_FILE_NAME}.exe
                           DEPENDS ${CPACK_PACKAGE_FILE_NAME}.exe virtio-win.iso)
        add_custom_target(guest_tools_iso DEPENDS ${CPACK_PACKAGE_FILE_NAME}.iso)
    else (WIN32)
        set(CPACK_PACKAGE_VENDOR "Flexible Software Solutions")
        set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
        set(CPACK_PACKAGE_VERSION_MAJOR ${FLEXVDI_VERSION_MAJOR})
        set(CPACK_PACKAGE_VERSION_MINOR ${FLEXVDI_VERSION_MINOR})
        set(CPACK_PACKAGE_VERSION_PATCH ${FLEXVDI_VERSION_PATCH})
        set(CPACK_GENERATOR "STGZ")
        set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
        set(CPACK_COMPONENTS_ALL Unspecified)
        set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
        set(CPACK_COMPONENT_UNSPECIFIED_DISPLAY_NAME "${FLEXVDI_VERSION}")
        set(CPACK_STGZ_USE_DISPLAY_NAME_IN_FILENAME ON)
        include (CPack)
    endif ()
endif ()
