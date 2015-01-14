include_directories(${CMAKE_CURRENT_SOURCE_DIR}/winddk)
add_definitions(-DNT50)
set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/redmon32.def")
add_library(redmon SHARED redmon.c portmon.c)
target_link_libraries(redmon winspool spoolss psapi userenv)
set_target_properties(redmon PROPERTIES LINK_FLAGS
    "-Wl,-enable-stdcall-fixup ${DEF_FILE} -static-libgcc -static-libstdc++"
    PREFIX "")
