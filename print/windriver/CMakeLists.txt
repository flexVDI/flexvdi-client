include_directories(${CMAKE_CURRENT_SOURCE_DIR}/winddk)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/gs9.15)
add_definitions(-DNT50)
set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/redmon.def")
add_library(redmon SHARED redmon.c portmon.c redmon.rc)
target_link_libraries(redmon psapi userenv)
set_target_properties(redmon PROPERTIES LINK_FLAGS
    "-Wl,-enable-stdcall-fixup ${DEF_FILE} -static-libgcc -static-libstdc++"
    PREFIX "")

add_executable(setredmon setredmon.cpp)
add_executable(unredmon unredmon.cpp)
add_executable(filter filter.cpp $<TARGET_OBJECTS:util_o>)
target_link_libraries(filter gsdll${WIN_BITS})
