add_executable(flexvdiprint flexvdiprint.cpp $<TARGET_OBJECTS:util_o>)
target_link_libraries(flexvdiprint ${Boost_LIBRARIES})
