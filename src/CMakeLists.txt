set(client_sources flexvdi-spice.c flexvdi-cmdline.c)
if (WIN32)
    if (ENABLE_PRINTING)
        set(client_sources ${client_sources} printclient.c PPDGenerator.c printclient-win32.c)
        add_executable(print-pdf print-with-poppler.c)
        target_link_libraries(print-pdf ${CLIENT_LIBRARIES})
    endif ()
else ()
    if (ENABLE_PRINTING)
        set(client_sources ${client_sources} printclient.c PPDGenerator.c printclient-cups.c)
    endif ()
    if (ENABLE_SERIALREDIR)
        set(client_sources ${client_sources} serialredir.c)
    endif ()
endif ()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/flexvdi-port.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/flexvdi-port.h")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
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
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/flexvdi-port.h" DESTINATION include)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/flexvdi-spice-client.pc"
        DESTINATION lib/pkgconfig)
