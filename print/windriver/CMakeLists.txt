include_directories(${CMAKE_CURRENT_SOURCE_DIR}/winddk)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/gs9.15)
add_definitions(-DNT50)
set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/redmon.def")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/setup.nsi.in"
               "${CMAKE_CURRENT_BINARY_DIR}/setup.nsi" @ONLY)
add_library(redmon SHARED redmon.c portmon.c redmon.rc)
target_link_libraries(redmon psapi userenv)
set_target_properties(redmon PROPERTIES LINK_FLAGS
    "-Wl,-enable-stdcall-fixup ${DEF_FILE} -static"
    PREFIX "")

# The following targets must be 32-bit
if (WIN_BITS EQUAL 32)
add_library(setup SHARED setup.cpp)
set_target_properties(setup PROPERTIES
    LINK_FLAGS "-static"
    PREFIX "")
add_executable(filter WIN32 filter.cpp ../send-job.cpp $<TARGET_OBJECTS:util_o>)
target_link_libraries(filter ${FLEXVDI_LIBRARIES} gsdll32)
endif (WIN_BITS EQUAL 32)
