cmake_minimum_required(VERSION 2.8)

# Project information
project(flexvm_agent)
set(FLEXVDI_VERSION 2.1.1)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
message("flexvdi-guest-agent v${FLEXVDI_VERSION} ${CMAKE_BUILD_TYPE} build")

# Find required packages
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")
include(FindPkgConfig)
pkg_search_module(spice_protocol REQUIRED spice-protocol)
find_package(Boost 1.50.0 REQUIRED system thread)
include_directories("${spice_protocol_INCLUDE_DIRS}" "${Boost_INCLUDE_DIRS}")
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)

# Build options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(CMAKE_EXE_LINKER_FLAGS "-static")
if (CMAKE_BUILD_TYPE STREQUAL Release)
    set(CMAKE_EXE_LINKER_FLAGS "-static -s")
endif (CMAKE_BUILD_TYPE STREQUAL Release)
if (WIN32)
    set (Boost_LIBRARIES "${Boost_LIBRARIES}" wsock32)
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
add_library(util_o OBJECT util.cpp)
add_library(objects OBJECT ${sources})

# Build guest agent
if (WIN32)
    configure_file("${PROJECT_SOURCE_DIR}/flexvdi-guest-agent.rc"
                   "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc" @ONLY)
    add_executable(flexvdi-guest-agent WindowsService.cpp
                   "${PROJECT_BINARY_DIR}/flexvdi-guest-agent.rc"
                   $<TARGET_OBJECTS:objects> $<TARGET_OBJECTS:util_o>)
    configure_file("${PROJECT_SOURCE_DIR}/setup.nsi" "${PROJECT_BINARY_DIR}/setup.nsi" @ONLY)
else (WIN32)
    add_executable(flexvdi-guest-agent main.cpp
                   $<TARGET_OBJECTS:objects> $<TARGET_OBJECTS:util_o>)
    install(TARGETS flexvdi-guest-agent RUNTIME DESTINATION bin)
endif (WIN32)
target_link_libraries(flexvdi-guest-agent "${Boost_LIBRARIES}" pthread)

# Build components
add_subdirectory(cred-connectors)
add_subdirectory(print)
add_subdirectory(client)
add_subdirectory(test)
