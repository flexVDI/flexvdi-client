set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL True)
include_directories(${PROJECT_SOURCE_DIR})
add_executable(get_printer_ppds get_printer_ppds.c $<TARGET_OBJECTS:client_objects>)
target_link_libraries(get_printer_ppds ${CLIENT_LIBRARIES} m)
