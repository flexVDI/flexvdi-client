set (sources flexvdi-spice.c flexvdi-cmdline.c printclient.c PPDGenerator.c)
if (WIN32)
    set (sources ${sources} printclient-win32.c)
else ()
    set (sources ${sources} serialredir.c printclient-cups.c)
endif ()

add_library(flexvdi-spice-client SHARED ${sources})
target_link_libraries(flexvdi-spice-client ${CLIENT_LIBRARIES})
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/flexvdi-spice-client.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/flexvdi-spice-client.pc" @ONLY)

install(TARGETS flexvdi-spice-client RUNTIME DESTINATION bin COMPONENT client
                                     LIBRARY DESTINATION lib COMPONENT client)
install(FILES flexvdi-port.h DESTINATION include COMPONENT client)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/flexvdi-spice-client.pc"
        DESTINATION lib/pkgconfig COMPONENT client)

add_custom_target(install_client ${CMAKE_COMMAND} -DCOMPONENT=client
                                 -P ${PROJECT_BINARY_DIR}/cmake_install.cmake)
