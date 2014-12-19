add_executable(send_credentials send_credentials.cpp)
target_link_libraries(send_credentials "${Boost_LIBRARIES}" "${WINSOCK_LIBRARIES}" pthread)
add_executable(testginalib testginalib.cpp)
