add_definitions(-DBOOST_TEST_DYN_LINK)
add_executable(unit-tests unit-main.cpp
               unit-MessageBuffer.cpp
               unit-Connection.cpp
               unit-LocalPipe.cpp
               unit-DispatcherRegistry.cpp
               $<TARGET_OBJECTS:agent_objects> $<TARGET_OBJECTS:util_o>)
target_link_libraries(unit-tests ${FLEXVDI_LIBRARIES}
                      ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_executable(send_credentials send_credentials.cpp
               ../credentials-manager/CredentialsThread.cpp
               $<TARGET_OBJECTS:util_o>)
target_link_libraries(send_credentials ${FLEXVDI_LIBRARIES})

if (WIN32)
    add_executable(testginalib testginalib.cpp)
    add_executable(read_named_pipe read_named_pipe.cpp)
    set (get_printer_ppds_sources get_printer_ppds.c ../client/flexvdi-spice.c
         ../client/printclient.c ../client/PPDGenerator.c ../client/printclient-win32.c)
else (WIN32)
    set (get_printer_ppds_sources get_printer_ppds.c ../client/flexvdi-spice.c
         ../client/printclient.c ../client/PPDGenerator.c ../client/printclient-cups.c)
endif (WIN32)
if (BUILD_CLIENT)
    add_executable(get_printer_ppds ${get_printer_ppds_sources})
    target_link_libraries(get_printer_ppds ${CLIENT_LIBRARIES} m)
endif ()
