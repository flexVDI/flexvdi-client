set(client_sources
    main.c client-app.c client-win.c client-conn.c
    flexvdi-port.c configuration.c client-request.c
    spice-win.c printclient.c PPDGenerator.c)
if (WIN32)
    set(client_sources ${client_sources}
        terminalid-win32.c printclient-win32.c)
    set(CLIENT_LIBRARIES ${CLIENT_LIBRARIES} iphlpapi)
else ()
    set(client_sources ${client_sources}
        terminalid-linux.c printclient-cups.c serialredir.c)
endif ()

add_definitions(-DG_LOG_DOMAIN=\"flexvdi\" -DVERSION_STRING=\"${FLEXVDI_VERSION}\")

add_library(client_objects OBJECT ${client_sources})
set_target_properties(client_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)
add_executable(flexvdi-client $<TARGET_OBJECTS:client_objects>
               $<TARGET_OBJECTS:resource_objects>)
target_link_libraries(flexvdi-client ${CLIENT_LIBRARIES} m)

install(TARGETS flexvdi-client RUNTIME DESTINATION bin)
