set(lib_sources
    client-conn.c client-log.c flexvdi-port.c configuration.c client-request.c
    printclient.c PPDGenerator.c)
set(lib_headers
    client-conn.h client-log.h flexvdi-port.h configuration.h client-request.h
    printclient.h)
set(client_sources client-app.c client-win.c spice-win.c about.c)

if (WIN32)
    add_custom_target(ico_icon
                      COMMAND convert "${PROJECT_SOURCE_DIR}/resources/images/icon.png" icon.ico
                      DEPENDS "${PROJECT_SOURCE_DIR}/resources/images/icon.png")
    file(WRITE icon.rc "IDI_ICON1 ICON DISCARDABLE \"${CMAKE_CURRENT_BINARY_DIR}/icon.ico\"")
    set(lib_sources ${lib_sources}
        terminalid-win32.c printclient-win32.c)
    set(CLIENT_LIBRARIES ${CLIENT_LIBRARIES} iphlpapi)
    set(ICON icon.rc)
elseif (APPLE)
    set(lib_sources ${lib_sources}
        terminalid-macos.c printclient-cups.c)
    set(ICON "")
else ()
    set(lib_sources ${lib_sources}
        terminalid-linux.c printclient-cups.c serialredir.c)
    set(lib_headers ${lib_headers} serialredir.h)
    set(ICON "")
endif ()

add_definitions(-DG_LOG_DOMAIN=\"flexvdi\" -DVERSION_STRING=\"${FLEXVDI_VERSION}\")

add_library(flexvdi-client SHARED ${lib_sources})
target_link_libraries(flexvdi-client ${CLIENT_LIBRARIES})
set_target_properties(flexvdi-client PROPERTIES PUBLIC_HEADER "${lib_headers}")

add_library(client_objects OBJECT ${client_sources})
set_target_properties(client_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)

add_executable(flexvdi-client-bin WIN32 main.c ${ICON} $<TARGET_OBJECTS:client_objects>
               $<TARGET_OBJECTS:resource_objects>)
set_target_properties(flexvdi-client-bin
                      PROPERTIES OUTPUT_NAME flexvdi-client)

if (WIN32)
    add_dependencies(flexvdi-client-bin ico_icon)
elseif (APPLE)
    target_compile_options(client_objects PRIVATE -xobjective-c -mmacosx-version-min=10.10)
    target_link_libraries(flexvdi-client "-framework Cocoa")
endif ()
target_link_libraries(flexvdi-client-bin flexvdi-client ${CLIENT_LIBRARIES} m)

install(TARGETS flexvdi-client-bin flexvdi-client
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/flexvdi-client)
