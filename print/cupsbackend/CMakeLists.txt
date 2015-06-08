add_executable(flexvdiprint flexvdiprint.cpp ../send-job.cpp $<TARGET_OBJECTS:util_o>)
target_link_libraries(flexvdiprint ${FLEXVDI_LIBRARIES})
set(GUEST_TOOLS_TARGETS ${GUEST_TOOLS_TARGETS} flexvdiprint PARENT_SCOPE)
install(TARGETS flexvdiprint RUNTIME DESTINATION libexec/flexvdi-guest-agent)
install(FILES pdftopdf-nocopies DESTINATION libexec/flexvdi-guest-agent
        PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
install(FILES "${PROJECT_SOURCE_DIR}/print/flexvdips.ppd" DESTINATION share/flexvdi-guest-agent)
