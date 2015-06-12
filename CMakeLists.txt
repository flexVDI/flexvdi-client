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

# Find required packages
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")
include(FindPkgConfig)
pkg_check_modules(spice_protocol REQUIRED spice-protocol)
include_directories(${spice_protocol_INCLUDE_DIRS})
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)

# Find client packages
set(BUILD_CLIENT True)
include(CheckFunctionExists)
check_function_exists(strnlen HAVE_STRNLEN)
pkg_check_modules(spice_glib spice-client-glib-2.0)
if (WIN32)
    pkg_check_modules(CAIRO cairo)
    pkg_check_modules(POPPLER poppler-glib)
    if (NOT spice_glib_FOUND OR NOT CAIRO_FOUND OR NOT POPPLER_FOUND)
        message("Spice client, Cairo and Poppler are needed to build the client library, skipping it.")
        set(BUILD_CLIENT False)
    else ()
        set(CLIENT_LIBRARY_DIRS ${spice_glib_LIBRARY_DIRS} ${CAIRO_LIBRARY_DIRS} ${POPPLER_LIBRARY_DIRS})
        set(CLIENT_INCLUDE_DIRS ${spice_glib_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS} ${POPPLER_INCLUDE_DIRS})
        set(CLIENT_LIBRARIES ${spice_glib_LIBRARIES} ${CAIRO_LIBRARIES} ${POPPLER_LIBRARIES})
    endif ()
else ()
    pkg_check_modules(gio_unix gio-unix-2.0)
    find_package(Cups)
    if (NOT spice_glib_FOUND OR NOT gio_unix_FOUND OR NOT CUPS_FOUND)
        message("Spice client, Cups and GIO-unix are needed to build the client library, skipping it.")
        set(BUILD_CLIENT False)
    else ()
        set(CLIENT_LIBRARY_DIRS ${spice_glib_LIBRARY_DIRS} ${gio_unix_LIBRARY_DIRS})
        set(CLIENT_INCLUDE_DIRS ${spice_glib_INCLUDE_DIRS} ${gio_unix_INCLUDE_DIRS} ${CUPS_INCLUDE_DIR})
        set(CLIENT_LIBRARIES ${spice_glib_LIBRARIES} ${gio_unix_LIBRARIES} ${CUPS_LIBRARIES})
    endif ()
endif ()
include_directories(${CLIENT_INCLUDE_DIRS})
link_directories(${CLIENT_LIBRARY_DIRS})

# Find guest packages
set(BUILD_GUEST True)
if (NOT WIN32)
    set(Boost_USE_STATIC_LIBS ON)
endif ()
find_package(Boost 1.49.0 COMPONENTS system thread locale unit_test_framework)
if (WIN32)
    if (NOT Boost_FOUND)
        message("Boost >1.49 is needed to build the guest agent, skipping it")
        set(BUILD_GUEST False)
    else ()
        set(FLEXVDI_LIBRARIES ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY}
                              pthread wsock32 setupapi)
        include_directories(${Boost_INCLUDE_DIRS})
        link_directories(${Boost_LIBRARY_DIRS})
    endif ()
else ()
    set(SELINUX_MAKEFILE /usr/share/selinux/devel/Makefile)
    if (NOT Boost_FOUND OR NOT CUPS_FOUND)
        message("Boost >1.49 and Cups are needed to build the guest agent, skipping it")
        set(BUILD_GUEST False)
    else ()
        set(FLEXVDI_LIBRARIES ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY}
                              ${CUPS_LIBRARIES} pthread)
        include_directories(${Boost_INCLUDE_DIRS} ${CUPS_INCLUDE_DIR})
        link_directories(${Boost_LIBRARY_DIRS})
    endif ()
endif ()

if (NOT BUILD_CLIENT AND NOT BUILD_GUEST)
    message(FATAL_ERROR "Not building client nor guest")
endif ()

# Build options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
if (CMAKE_BUILD_TYPE STREQUAL Release)
    set(CMAKE_EXE_LINKER_FLAGS "-s")
endif (CMAKE_BUILD_TYPE STREQUAL Release)
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
    # WINVER = 0x0501 -> Windows XP
    add_definitions(-DWIN32 -DUNICODE -D_UNICODE -DWINVER=0x0501 -DWIN32_LEAN_AND_MEAN)
    string(REPLACE "." "," FLEXVDI_VERSION_COMA "${FLEXVDI_VERSION}")
endif ()

# Sources and objects
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories(${PROJECT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
set(agent_sources FlexVDIGuestAgent.cpp DispatcherRegistry.cpp
                  Connection.cpp VirtioPort.cpp LocalPipe.cpp)

# Build guest agent
if (BUILD_GUEST)
    set(GUEST_TOOLS_TARGETS)
    add_subdirectory(credentials-manager)
    add_subdirectory(print)

    add_library(agent_objects OBJECT ${agent_sources})
    if (WIN32)
        add_library(util_o OBJECT util.cpp WinSpoolImpl.cpp)
        configure_file("${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.rc"
                       "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc" @ONLY)
        add_executable(flexvdi-guest-agent WindowsService.cpp
                       "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc"
                       $<TARGET_OBJECTS:agent_objects> $<TARGET_OBJECTS:util_o>)
        set_target_properties(flexvdi-guest-agent PROPERTIES LINK_FLAGS "-static")
        target_link_libraries(flexvdi-guest-agent ${FLEXVDI_LIBRARIES})
        set(SPICE_GUEST_TOOLS spice-guest-tools-0.100.exe)
        configure_file("${PROJECT_SOURCE_DIR}/setup.nsi.in"
                       "${PROJECT_BINARY_DIR}/setup.nsi" @ONLY)
    else (WIN32)
        add_library(util_o OBJECT util.cpp)
        add_executable(flexvdi-guest-agent main.cpp
                       $<TARGET_OBJECTS:agent_objects> $<TARGET_OBJECTS:util_o>)
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
        add_custom_command(OUTPUT ${SPICE_GUEST_TOOLS}
                           COMMAND wget -nc http://www.spice-space.org/download/windows/spice-guest-tools/${SPICE_GUEST_TOOLS})
        add_custom_command(OUTPUT ${CPACK_PACKAGE_FILE_NAME}.exe
                           COMMAND "${PROJECT_SOURCE_DIR}/find_win_components.sh" ${WIN_COMPONENTS_ANCESTORS}
                           COMMAND echo Running makensis, see makensis.log for errors
                           COMMAND makensis -Omakensis.log setup.nsi
                           DEPENDS ${GUEST_TOOLS_TARGETS} ${SPICE_GUEST_TOOLS})
        add_custom_target(package DEPENDS ${CPACK_PACKAGE_FILE_NAME}.exe)
        add_custom_command(OUTPUT virtio-win.iso
                           COMMAND wget -nc https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/latest-virtio/virtio-win.iso)
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
