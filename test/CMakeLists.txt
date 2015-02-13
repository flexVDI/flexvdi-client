add_executable(unit-tests unit-main.cpp
               $<TARGET_OBJECTS:util_o>)
target_link_libraries(unit-tests "${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}")

add_executable(send_credentials send_credentials.cpp
               ../cred-connectors/CredentialsThread.cpp
               $<TARGET_OBJECTS:util_o>)
target_link_libraries(send_credentials "${FLEXVDI_LIBRARIES}")
if (WIN32)
    add_executable(testginalib testginalib.cpp)
    add_executable(read_named_pipe read_named_pipe.cpp)
else (WIN32)
endif (WIN32)
