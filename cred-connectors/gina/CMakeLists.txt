set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/flexVDIGina.def")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/setup.nsi.in"
               "${CMAKE_CURRENT_BINARY_DIR}/setup.nsi" @ONLY)
add_library(flexVDIGina SHARED flexVDIGina.cpp GinaDialogHooks.cpp
            ../CredentialsThread.cpp $<TARGET_OBJECTS:util_o>)
target_link_libraries(flexVDIGina "${Boost_LIBRARIES}")
set_target_properties(flexVDIGina PROPERTIES LINK_FLAGS
    "-Wl,-enable-stdcall-fixup ${DEF_FILE} -static"
    PREFIX "")
