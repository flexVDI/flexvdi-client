cmake_minimum_required(VERSION 2.8)

# Project information
project(flexvdi-guest-agent)
set(FLEXVDI_VERSION_MAJOR 2)
set(FLEXVDI_VERSION_MINOR 0)
set(FLEXVDI_VERSION_PATCH 1)
set(FLEXVDI_VERSION ${FLEXVDI_VERSION_MAJOR}.${FLEXVDI_VERSION_MINOR}.${FLEXVDI_VERSION_PATCH})
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
message("flexvdi-guest-agent v${FLEXVDI_VERSION} ${CMAKE_BUILD_TYPE} build")

# Find required packages
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")
include(FindPkgConfig)
pkg_search_module(spice_protocol REQUIRED spice-protocol)
find_package(Boost 1.50.0 REQUIRED COMPONENTS system thread unit_test_framework)
pkg_search_module(GLIB REQUIRED glib-2.0)
include_directories("${spice_protocol_INCLUDE_DIRS}" "${Boost_INCLUDE_DIRS}" "${GLIB_INCLUDE_DIRS}")
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
set(SELINUX_MAKEFILE /usr/share/selinux/devel/Makefile)
if (WIN32)
    pkg_search_module(CAIRO REQUIRED cairo)
    pkg_search_module(POPPLER REQUIRED poppler-glib)
    include_directories("${CAIRO_INCLUDE_DIRS}" "${POPPLER_INCLUDE_DIRS}")
else ()
    find_package(Cups REQUIRED)
    include_directories("${CUPS_INCLUDE_DIR}")
endif ()

# Build options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
if (CMAKE_BUILD_TYPE STREQUAL Release)
    set(CMAKE_EXE_LINKER_FLAGS "-s")
endif (CMAKE_BUILD_TYPE STREQUAL Release)
set (FLEXVDI_LIBRARIES "${Boost_THREAD_LIBRARY}" "${Boost_SYSTEM_LIBRARY}" pthread)
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    set (FLEXVDI_LIBRARIES "${FLEXVDI_LIBRARIES}" wsock32)
    add_definitions(-DWIN32 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0600 -DWIN32_LEAN_AND_MEAN)
    string(REPLACE "." "," FLEXVDI_VERSION_COMA "${FLEXVDI_VERSION}")
endif (WIN32)

# Sources and objects
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
set(sources FlexVDIGuestAgent.cpp VirtioPort.cpp
    DispatcherRegistry.cpp Connection.cpp LocalPipe.cpp
    CredentialsManager.cpp
    PrintManager.cpp
    )
add_library(objects OBJECT ${sources})

# Build guest agent
if (WIN32)
    add_library(util_o OBJECT util.cpp WinSpoolImpl.cpp)
    configure_file("${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.rc"
                   "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc" @ONLY)
    add_executable(flexvdi-guest-agent WindowsService.cpp
                   "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc"
                   $<TARGET_OBJECTS:objects> $<TARGET_OBJECTS:util_o>)
    target_link_libraries(flexvdi-guest-agent "${FLEXVDI_LIBRARIES}")
    configure_file("${PROJECT_SOURCE_DIR}/setup.nsi.in"
                   "${PROJECT_BINARY_DIR}/setup.nsi" @ONLY)
else (WIN32)
    add_library(util_o OBJECT util.cpp)
    add_executable(flexvdi-guest-agent main.cpp
                   $<TARGET_OBJECTS:objects> $<TARGET_OBJECTS:util_o>)
    target_link_libraries(flexvdi-guest-agent "${FLEXVDI_LIBRARIES}")
    configure_file("${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.te.in"
                   "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.te")
    add_custom_command(OUTPUT flexvdi-guest-agent.pp
                       COMMAND make -f "${SELINUX_MAKEFILE}" flexvdi-guest-agent.pp
                       MAIN_DEPENDENCY "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.te"
                       WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
    add_custom_target(flexvdi-selinux-policy ALL DEPENDS flexvdi-guest-agent.pp)
    install(TARGETS flexvdi-guest-agent RUNTIME DESTINATION bin)
    install(FILES "${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.service"
            DESTINATION lib/systemd/system)
    install(FILES "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.pp"
            DESTINATION share/selinux/targeted)
endif (WIN32)

# Build components
add_subdirectory(cred-connectors)
add_subdirectory(print)
add_subdirectory(client)
add_subdirectory(test)

# CPack commands
set(CPACK_PACKAGE_FILE_NAME "flexvdi-guest-tools-${FLEXVDI_VERSION}")
set(CPACK_SOURCE_GENERATOR "TBZ2")
execute_process(COMMAND git status -unormal --porcelain --ignored
                COMMAND awk "/^[?!][?!]/ {printf \";/%s\", substr($0, 4)}"
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_UNTRACKED)
set(CPACK_SOURCE_IGNORE_FILES "/.git/;.*swp;.*~;/.gitignore${GIT_UNTRACKED}")
set(CPACK_PACKAGE_VENDOR "Flexible Software Solutions")
# set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR ${FLEXVDI_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${FLEXVDI_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${FLEXVDI_VERSION_PATCH})
if (WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_WORKING_DIR "${PROJECT_BINARY_DIR}")
else (WIN32)
    set(CPACK_GENERATOR "STGZ")
endif (WIN32)

include (CPack)
