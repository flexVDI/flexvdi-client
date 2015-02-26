if (WIN32)
    set (sources flexvdi-spice.c printclient.c printclient-win32.c PPDGenerator.c)
else ()
    set (sources flexvdi-spice.c printclient.c printclient-cups.c PPDGenerator.c)
endif ()

add_library(flexvdi-spice-client SHARED ${sources})
target_link_libraries(flexvdi-spice-client ${GLIB_LIBRARIES})
