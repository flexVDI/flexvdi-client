cmake_minimum_required(VERSION 2.8)
project(flexvm_agent)

include(FindPkgConfig)
pkg_search_module(spice_protocol REQUIRED spice-protocol)
include_directories("${spice_protocol_INCLUDE_DIRS}")

include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Werror -Wall")
set(sources FlexVMAgent.cpp VirtioPort.cpp FlexVMProto.c CredentialsManager.cpp
    DispatcherRegistry.cpp)

add_library(objects OBJECT ${sources})
add_executable(flexvm_agent main.cpp $<TARGET_OBJECTS:objects>)

install(TARGETS flexvm_agent RUNTIME DESTINATION bin)

add_subdirectory(test)
