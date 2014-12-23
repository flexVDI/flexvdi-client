set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/flexVDIGina.def")
add_library(flexVDIGina SHARED flexVDIGina.cpp GinaDialogHooks.cpp
            CredentialsConnectorThread.cpp ../../util.cpp)
set_target_properties(flexVDIGina PROPERTIES LINK_FLAGS
    "-Wl,-enable-stdcall-fixup ${DEF_FILE} -static-libgcc -static-libstdc++")
