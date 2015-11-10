set(client_sources flexvdi-spice.c flexvdi-cmdline.c printclient.c PPDGenerator.c)
if (WIN32)
    set(client_sources ${client_sources} printclient-win32.c)
    add_executable(print-pdf print-with-poppler.c)
    target_link_libraries(print-pdf ${CLIENT_LIBRARIES})
else ()
    set(client_sources ${client_sources} serialredir.c printclient-cups.c)
endif ()

add_definitions(-DG_LOG_DOMAIN=\"flexvdi\")
add_library(client_objects OBJECT ${client_sources})
set_target_properties(client_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)
add_library(flexvdi-spice-client SHARED $<TARGET_OBJECTS:client_objects>)
target_link_libraries(flexvdi-spice-client ${CLIENT_LIBRARIES})
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/flexvdi-spice-client.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/flexvdi-spice-client.pc" @ONLY)

install(TARGETS flexvdi-spice-client RUNTIME DESTINATION bin
                                     ARCHIVE DESTINATION lib
                                     LIBRARY DESTINATION lib)
install(FILES flexvdi-port.h DESTINATION include)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/flexvdi-spice-client.pc"
        DESTINATION lib/pkgconfig)
