cmake_minimum_required(VERSION 2.8)

# Project information
project(flexvdi-guest-tools)
set(FLEXVDI_VERSION_MAJOR 2)
set(FLEXVDI_VERSION_MINOR 0)
set(FLEXVDI_VERSION_PATCH 3)
set(FLEXVDI_VERSION ${FLEXVDI_VERSION_MAJOR}.${FLEXVDI_VERSION_MINOR}.${FLEXVDI_VERSION_PATCH})
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
message("${CMAKE_PROJECT_NAME} v${FLEXVDI_VERSION} ${CMAKE_BUILD_TYPE} build")

# Find required/optional packages
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")
include(FindPkgConfig)
pkg_search_module(spice_protocol REQUIRED spice-protocol)
if (NOT WIN32)
    set(Boost_USE_STATIC_LIBS ON)
endif ()
find_package(Boost 1.49.0 REQUIRED COMPONENTS system thread locale unit_test_framework)
include_directories(${spice_protocol_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
pkg_search_module(spice_glib spice-client-glib-2.0)
pkg_search_module(spice_gtk spice-client-gtk-3.0)
include_directories(${spice_glib_INCLUDE_DIRS} ${spice_gtk_INCLUDE_DIRS})
# NOTE: At least, the following line won't work from inside the if block!!
link_directories(${spice_glib_LIBRARY_DIRS} ${spice_gtk_LIBRARY_DIRS})
if (spice_glib_FOUND AND spice_gtk_FOUND)
    set(BUILD_CLIENT True)
endif ()
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
set(SELINUX_MAKEFILE /usr/share/selinux/devel/Makefile)
if (WIN32)
    pkg_search_module(CAIRO cairo)
    pkg_search_module(POPPLER poppler-glib)
    if (NOT CAIRO_FOUND OR NOT POPPLER_FOUND)
        message("Cairo and Poppler are needed to build the client library, skipping it.")
        set(BUILD_CLIENT False)
    endif ()
    include_directories(${CAIRO_INCLUDE_DIRS} ${POPPLER_INCLUDE_DIRS})
    set(CLIENT_LIBRARIES ${spice_glib_LIBRARIES} ${spice_gtk_LIBRARIES}
                         ${CAIRO_LIBRARIES} ${POPPLER_LIBRARIES})
else ()
    find_package(Cups REQUIRED)
    include_directories(${CUPS_INCLUDE_DIR})
    set(CLIENT_LIBRARIES ${spice_glib_LIBRARIES} ${spice_gtk_LIBRARIES} ${CUPS_LIBRARIES})
endif ()

# Build options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
if (CMAKE_BUILD_TYPE STREQUAL Release)
    set(CMAKE_EXE_LINKER_FLAGS "-s")
endif (CMAKE_BUILD_TYPE STREQUAL Release)
set(FLEXVDI_LIBRARIES ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} pthread)
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
    set(FLEXVDI_LIBRARIES "${FLEXVDI_LIBRARIES}" wsock32 setupapi)
    add_definitions(-DWIN32 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0600 -DWIN32_LEAN_AND_MEAN)
    string(REPLACE "." "," FLEXVDI_VERSION_COMA "${FLEXVDI_VERSION}")
else ()
    set (FLEXVDI_LIBRARIES ${FLEXVDI_LIBRARIES} ${CUPS_LIBRARIES})
endif ()

# Sources and objects
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories(${PROJECT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
set(agent_sources FlexVDIGuestAgent.cpp DispatcherRegistry.cpp
                  Connection.cpp VirtioPort.cpp LocalPipe.cpp)

# Build components
add_subdirectory(credentials-manager)
add_subdirectory(print)

# Build guest agent
add_library(agent_objects OBJECT ${agent_sources})
if (WIN32)
    add_library(util_o OBJECT util.cpp WinSpoolImpl.cpp)
    set(SPICE_GUEST_TOOLS spice-guest-tools-0.74.exe)
    add_custom_command(OUTPUT ${SPICE_GUEST_TOOLS}
                       COMMAND wget -nc http://www.spice-space.org/download/windows/spice-guest-tools/${SPICE_GUEST_TOOLS}
                       WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
    configure_file("${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.rc"
                   "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc" @ONLY)
    add_executable(flexvdi-guest-agent WindowsService.cpp
                   "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc"
                   "${PROJECT_BINARY_DIR}/${SPICE_GUEST_TOOLS}"
                   $<TARGET_OBJECTS:agent_objects> $<TARGET_OBJECTS:util_o>)
    set_target_properties(flexvdi-guest-agent PROPERTIES LINK_FLAGS "-static")
    target_link_libraries(flexvdi-guest-agent ${FLEXVDI_LIBRARIES})
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

# Build other
if (BUILD_CLIENT)
    add_subdirectory(client)
endif ()
add_subdirectory(test)

# CPack commands
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${FLEXVDI_VERSION}")
add_custom_target(dist
                  COMMAND git archive --prefix=${CPACK_PACKAGE_FILE_NAME}/
                          -o "${PROJECT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.tar.bz2" HEAD
                  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
set(CPACK_PACKAGE_VENDOR "Flexible Software Solutions")
# set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR ${FLEXVDI_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${FLEXVDI_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${FLEXVDI_VERSION_PATCH})
if (WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_WORKING_DIR "${PROJECT_BINARY_DIR}")
    configure_file("${PROJECT_SOURCE_DIR}/autorun.inf.in"
                   "${PROJECT_BINARY_DIR}/autorun.inf")
    add_custom_target(guest_tools_iso
                      COMMAND mkisofs -J -r -V "flexVDI Guest Tools v${FLEXVDI_VERSION}"
                                      -o ${CPACK_PACKAGE_FILE_NAME}.iso
                                      autorun.inf ${CPACK_PACKAGE_FILE_NAME}.exe
                      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
else (WIN32)
    set(CPACK_GENERATOR "STGZ")
    set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_ALL Unspecified)
    set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
    set(CPACK_COMPONENT_UNSPECIFIED_DISPLAY_NAME "${FLEXVDI_VERSION}")
    set(CPACK_STGZ_USE_DISPLAY_NAME_IN_FILENAME ON)
endif (WIN32)

include (CPack)
