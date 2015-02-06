add_executable(flexvdiprint flexvdiprint.cpp ../send-job.cpp $<TARGET_OBJECTS:util_o>)
target_link_libraries(flexvdiprint ${Boost_LIBRARIES})
install(TARGETS flexvdiprint RUNTIME DESTINATION libexec/flexvdi-guest-agent)
install(FILES "${PROJECT_SOURCE_DIR}/print/flexvdips.ppd" DESTINATION share/flexvdi-guest-agent)
