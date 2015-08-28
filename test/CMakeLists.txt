if (BUILD_GUEST)
    add_executable(unit-tests unit-main.cpp
                   unit-MessageBuffer.cpp
                   unit-Connection.cpp
                   unit-LocalPipe.cpp
                   unit-DispatcherRegistry.cpp

                   unit-CredentialsThread.cpp
                   unit-CredentialsManager.cpp
                   ../credentials-manager/CredentialsThread.cpp

                   $<TARGET_OBJECTS:agent_objects> $<TARGET_OBJECTS:common_objects>)
    target_link_libraries(unit-tests ${FLEXVDI_LIBRARIES}
                          ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
    if (WIN32)
        set_target_properties(unit-tests PROPERTIES LINK_FLAGS "-static")
        add_executable(testginalib testginalib.cpp)
    endif ()
endif ()

if (BUILD_CLIENT)
    if (WIN32)
        set (get_printer_ppds_sources ../client/printclient-win32.c)
    else ()
        set (get_printer_ppds_sources ../client/serialredir.c ../client/printclient-cups.c)
    endif ()
    add_executable(get_printer_ppds get_printer_ppds.c
                   ../client/flexvdi-spice.c ../client/flexvdi-cmdline.c
                   ../client/printclient.c ../client/PPDGenerator.c
                   ${get_printer_ppds_sources})
    target_link_libraries(get_printer_ppds ${CLIENT_LIBRARIES} m)
endif ()
