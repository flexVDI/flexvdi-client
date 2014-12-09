cmake_minimum_required(VERSION 2.6)
project(flexvm_agent)

add_executable(flexvm_agent main.cpp)

install(TARGETS flexvm_agent RUNTIME DESTINATION bin)
