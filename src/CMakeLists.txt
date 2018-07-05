set(client_sources
    main.c client-app.c client-win.c client-conn.c
    flexvdi-spice.c configuration.c client-request.c
    spice-win.c)
if (WIN32)
    set(client_sources ${client_sources} terminalid-win32.c)
    set(CLIENT_LIBRARIES ${CLIENT_LIBRARIES} iphlpapi)
    if (ENABLE_PRINTING)
        set(client_sources ${client_sources} printclient.c PPDGenerator.c printclient-win32.c)
        add_executable(print-pdf print-with-poppler.c)
        target_link_libraries(print-pdf ${CLIENT_LIBRARIES})
        install(TARGETS print-pdf RUNTIME DESTINATION bin)
    endif ()
else ()
    set(client_sources ${client_sources} terminalid-linux.c)
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
add_definitions(-DG_LOG_DOMAIN=\"flexvdi\" -DVERSION_STRING=\"${FLEXVDI_VERSION}\")

add_library(client_objects OBJECT ${client_sources})
set_target_properties(client_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)
add_executable(flexvdi-client $<TARGET_OBJECTS:client_objects>
               $<TARGET_OBJECTS:resource_objects>)
target_link_libraries(flexvdi-client ${CLIENT_LIBRARIES} m)

install(TARGETS flexvdi-client RUNTIME DESTINATION bin)
