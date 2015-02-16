add_executable(unit-tests unit-main.cpp
               unit-MessageBuffer.cpp
               unit-Connection.cpp
               unit-LocalPipe.cpp
               unit-DispatcherRegistry.cpp
               $<TARGET_OBJECTS:objects> $<TARGET_OBJECTS:util_o>)
target_link_libraries(unit-tests "${FLEXVDI_LIBRARIES}"
                      "${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}")

add_executable(send_credentials send_credentials.cpp
               ../cred-connectors/CredentialsThread.cpp
               $<TARGET_OBJECTS:util_o>)
target_link_libraries(send_credentials "${FLEXVDI_LIBRARIES}")
if (WIN32)
    add_executable(testginalib testginalib.cpp)
    add_executable(read_named_pipe read_named_pipe.cpp)
else (WIN32)
endif (WIN32)
