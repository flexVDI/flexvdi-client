if (WIN32)
    set (sources flexvdi-spice.c printclient.c printclient-win32.c)
else ()
    set (sources flexvdi-spice.c printclient.c printclient-cups.c)
endif ()

add_library(flexvdi-spice-client STATIC ${sources})
