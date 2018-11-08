set(client_sources
    client-app.c client-win.c client-conn.c
    flexvdi-port.c configuration.c client-request.c
    spice-win.c printclient.c PPDGenerator.c about.c)
if (WIN32)
    add_custom_target(ico_icon
                      COMMAND convert "${PROJECT_SOURCE_DIR}/resources/images/icon.png" icon.ico
                      DEPENDS "${PROJECT_SOURCE_DIR}/resources/images/icon.png")
    file(WRITE icon.rc "IDI_ICON1 ICON DISCARDABLE \"${CMAKE_CURRENT_BINARY_DIR}/icon.ico\"")
    set(client_sources ${client_sources}
        terminalid-win32.c printclient-win32.c)
    set(CLIENT_LIBRARIES ${CLIENT_LIBRARIES} iphlpapi)
    set(ICON icon.rc)
elseif (APPLE)
    set(client_sources ${client_sources}
        terminalid-macos.c printclient-cups.c)
    set(ICON "")
else ()
    set(client_sources ${client_sources}
        terminalid-linux.c printclient-cups.c serialredir.c)
    set(ICON "")
endif ()

add_definitions(-DG_LOG_DOMAIN=\"flexvdi\" -DVERSION_STRING=\"${FLEXVDI_VERSION}\")

add_library(client_objects OBJECT ${client_sources})
set_target_properties(client_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)
add_executable(flexvdi-client WIN32 main.c ${ICON} $<TARGET_OBJECTS:client_objects>
               $<TARGET_OBJECTS:resource_objects>)
if (WIN32)
    add_dependencies(flexvdi-client ico_icon)
elseif (APPLE)
    target_compile_options(client_objects PRIVATE -xobjective-c -mmacosx-version-min=10.10)
    target_link_libraries(flexvdi-client "-framework Cocoa")
endif ()
target_link_libraries(flexvdi-client ${CLIENT_LIBRARIES} m)

install(TARGETS flexvdi-client RUNTIME DESTINATION bin)
