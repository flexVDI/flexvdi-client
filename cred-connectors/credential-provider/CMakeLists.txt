# include(GenerateGUID)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/winsdk/include)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/winsdk/lib-${WIN_ARCH})
add_definitions(-D_WIN32_WINNT=0x0600)

set(FLEXVDI_GUID "858F113B-553C-5FD6-DEDB-A89065FBF768" CACHE INTERNAL "")
set(FLEXVDI_GUID_PARAMS "0x858F113B,0x553C,0x5FD6,0xDE,0xDB,0xA8,0x90,0x65,0xFB,0xF7,0x68" CACHE INTERNAL "")
# generate_guid(FLEXVDI_GUID)
# message("Generated GUID: ${FLEXVDI_GUID}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/guid.h.in" "${CMAKE_CURRENT_BINARY_DIR}/guid.h")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/setup.nsi.in"
               "${CMAKE_CURRENT_BINARY_DIR}/setup.nsi" @ONLY)

set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/FlexVDICredentialProvider.def")
add_library(flexVDICredentialProvider SHARED FlexCredential.cpp FlexProvider.cpp
            guid.cpp ../CredentialsThread.cpp FlexProviderFactory.cpp helpers.cpp
            resources.rc $<TARGET_OBJECTS:util_o>)
target_link_libraries(flexVDICredentialProvider shlwapi secur32 Uuid "${Boost_LIBRARIES}")
set_target_properties(flexVDICredentialProvider PROPERTIES
    LINK_FLAGS "-Wl,-enable-stdcall-fixup ${DEF_FILE} -static"
    PREFIX "")
