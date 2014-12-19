cmake_minimum_required(VERSION 2.8)
project(flexvm_agent)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
message("flexvdi-guest-agent ${CMAKE_BUILD_TYPE} build")
set(CMAKE_EXE_LINKER_FLAGS "-static")
if (CMAKE_BUILD_TYPE STREQUAL Release)
    set(CMAKE_EXE_LINKER_FLAGS "-static -s")
endif (CMAKE_BUILD_TYPE STREQUAL Release)

include(FindPkgConfig)
pkg_search_module(spice_protocol REQUIRED spice-protocol)
find_package(Boost 1.50.0 REQUIRED system thread)
include_directories("${spice_protocol_INCLUDE_DIRS}" "${Boost_INCLUDE_DIRS}")

set(FLEXVDI_VERSION 2.1.1)
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")

if (WIN32)
    set(WINSOCK_LIBRARIES wsock32)
endif (WIN32)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Werror -Wall")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
set(sources FlexVDIGuestAgent.cpp VirtioPort.cpp CredentialsManager.cpp
    DispatcherRegistry.cpp util.cpp)
add_library(objects OBJECT ${sources})
if (WIN32)
    add_executable(flexvdi-guest-agent WindowsService.cpp $<TARGET_OBJECTS:objects>)
else (WIN32)
    add_executable(flexvdi-guest-agent main.cpp $<TARGET_OBJECTS:objects>)
endif (WIN32)
target_link_libraries(flexvdi-guest-agent "${Boost_LIBRARIES}" "${WINSOCK_LIBRARIES}" pthread)

install(TARGETS flexvdi-guest-agent RUNTIME DESTINATION bin)

add_subdirectory(test)
add_subdirectory(cred-connectors)
