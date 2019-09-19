set(LIB_SOURCES
    client-conn.c client-log.c flexvdi-port.c configuration.c client-request.c
    printclient.c PPDGenerator.c ws-tunnel.c)
set(LIB_HEADERS
    client-conn.h client-log.h flexvdi-port.h configuration.h client-request.h
    printclient.h)
set(CLIENT_SOURCES client-app.c client-win.c spice-win.c about.c)

if (WIN32)
    add_custom_target(ico_icon
                      COMMAND convert "${PROJECT_SOURCE_DIR}/resources/images/icon.png" icon.ico
                      DEPENDS "${PROJECT_SOURCE_DIR}/resources/images/icon.png")
    file(WRITE icon.rc "IDI_ICON1 ICON DISCARDABLE \"${CMAKE_CURRENT_BINARY_DIR}/icon.ico\"")
    set(LIB_SOURCES ${LIB_SOURCES}
        terminalid-win32.c printclient-win32.c legacy-config-parser-win32)
    set(LIB_LIBRARIES ${LIB_LIBRARIES} iphlpapi ws2_32)
    set(ICON icon.rc)
elseif (ANDROID)
    set(LIB_SOURCES ${LIB_SOURCES}
        terminalid-null.c printclient-null.c legacy-config-parser-null)
    set(ICON "")
elseif (IOS)
    set(LIB_SOURCES ${LIB_SOURCES}
        terminalid-ios.c printclient-null.c legacy-config-parser-null)
    set(ICON "")
elseif (APPLE)
    set(LIB_SOURCES ${LIB_SOURCES}
        terminalid-macos.c printclient-cups.c legacy-config-parser-null)
    set(ICON "")
else ()
    set(LIB_SOURCES ${LIB_SOURCES}
        terminalid-linux.c printclient-cups.c serialredir.c legacy-config-parser-linux)
    set(LIB_HEADERS ${LIB_HEADERS} serialredir.h)
    set(ICON "")
endif ()

add_definitions(-DG_LOG_DOMAIN=\"flexvdi\" -DVERSION_STRING=\"${FLEXVDI_VERSION}\")

# libflexvdi-client
add_library(flexvdi-client SHARED ${LIB_SOURCES})
target_link_libraries(flexvdi-client ${LIB_LIBRARIES})
set_target_properties(flexvdi-client PROPERTIES PUBLIC_HEADER "${LIB_HEADERS}")
if (APPLE)
    target_compile_options(flexvdi-client PRIVATE -xobjective-c -mmacosx-version-min=10.10)
    target_link_libraries(flexvdi-client "-framework Cocoa")
endif ()

if (NOT ANDROID AND NOT IOS)

# client objects
add_library(client_objects OBJECT ${CLIENT_SOURCES})
set_target_properties(client_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)
if (APPLE)
    target_compile_options(client_objects PRIVATE -xobjective-c -mmacosx-version-min=10.10)
endif ()

# main binary
add_executable(flexvdi-client-bin WIN32 main.c ${ICON} $<TARGET_OBJECTS:client_objects>
               $<TARGET_OBJECTS:resource_objects>)
set_target_properties(flexvdi-client-bin
                      PROPERTIES OUTPUT_NAME flexvdi-client)
if (WIN32)
    add_dependencies(flexvdi-client-bin ico_icon)
elseif (APPLE)
    target_link_libraries(flexvdi-client-bin "-framework Cocoa")
endif ()
target_link_libraries(flexvdi-client-bin flexvdi-client ${CLIENT_LIBRARIES} m)

install(TARGETS flexvdi-client-bin flexvdi-client
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/flexvdi-client)

endif (NOT ANDROID AND NOT IOS)
