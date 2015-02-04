add_executable(send_credentials send_credentials.cpp
               ../cred-connectors/CredentialsThread.cpp
               $<TARGET_OBJECTS:util_o>)
target_link_libraries(send_credentials "${Boost_LIBRARIES}")
if (WIN32)
    add_executable(testginalib testginalib.cpp)
    add_executable(read_named_pipe read_named_pipe.cpp)
else (WIN32)
endif (WIN32)
