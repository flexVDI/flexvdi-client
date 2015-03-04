set (sources flexvdi-spice.c printclient.c PPDGenerator.c)
if (WIN32)
    set (sources ${sources} printclient-win32.c)
else ()
    set (sources ${sources} printclient-cups.c)
endif ()

add_library(flexvdi-spice-client SHARED ${sources})
target_link_libraries(flexvdi-spice-client ${GLIB_LIBRARIES} ${PRINT_LIBRARIES})
