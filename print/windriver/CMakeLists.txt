include_directories(${CMAKE_CURRENT_SOURCE_DIR}/winddk)
add_definitions(-DNT50 -DNT40)
set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/redmon.def")
add_library(redmon SHARED redmon.c portmon.c)
target_link_libraries(redmon psapi userenv)
set_target_properties(redmon PROPERTIES LINK_FLAGS
    "-Wl,-enable-stdcall-fixup ${DEF_FILE} -static-libgcc -static-libstdc++"
    PREFIX "")

add_executable(setredmon setredmon.cpp)
add_executable(unredmon unredmon.cpp)
