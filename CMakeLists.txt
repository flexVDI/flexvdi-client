cmake_minimum_required(VERSION 2.8)
project(flexvm_agent)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(sources FlexVMAgent.cpp VirtioPort.cpp)

add_library(objects OBJECT ${sources})
add_executable(flexvm_agent main.cpp $<TARGET_OBJECTS:objects>)

install(TARGETS flexvm_agent RUNTIME DESTINATION bin)
