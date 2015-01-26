find_package(Glib)

include_directories(${GLIB_INCLUDE_DIRS})
add_library(flexvdi-spice-client STATIC flexvdi-spice.c printclient.c)
