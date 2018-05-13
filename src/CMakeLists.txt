set(client_sources
    main.c flexvdi-client-app.c flexvdi-client-win.c
    flexvdi-spice.c flexvdi-cmdline.c)
if (WIN32)
    if (ENABLE_PRINTING)
        set(client_sources ${client_sources} printclient.c PPDGenerator.c printclient-win32.c)
        add_executable(print-pdf print-with-poppler.c)
        target_link_libraries(print-pdf ${CLIENT_LIBRARIES})
        install(TARGETS print-pdf RUNTIME DESTINATION bin)
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
add_executable(flexvdi-client $<TARGET_OBJECTS:client_objects>)
target_link_libraries(flexvdi-client ${CLIENT_LIBRARIES} m)

install(TARGETS flexvdi-client RUNTIME DESTINATION bin)
