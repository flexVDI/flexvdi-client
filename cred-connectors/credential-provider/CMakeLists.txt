include(GenerateGUID)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/winsdk/include)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/winsdk/lib-${WIN_ARCH})

set(FLEXVDI_GUID "858F113B-553C-5FD6-DEDB-A89065FBF768" CACHE INTERNAL "")
set(FLEXVDI_GUID_PARAMS "0x858F113B,0x553C,0x5FD6,0xDE,0xDB,0xA8,0x90,0x65,0xFB,0xF7,0x68" CACHE INTERNAL "")
# generate_guid(FLEXVDI_GUID)
# message("Generated GUID: ${FLEXVDI_GUID}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/guid.h.in" "${CMAKE_CURRENT_BINARY_DIR}/guid.h")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/register.reg.in"
               "${CMAKE_CURRENT_BINARY_DIR}/register.reg")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/unregister.reg.in"
               "${CMAKE_CURRENT_BINARY_DIR}/unregister.reg")

add_definitions(-DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0600)
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wno-unknown-pragmas")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")

set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/FlexVDICredentialProvider.def")
add_library(FlexVDICredentialProvider SHARED CFlexCredential.cpp CFlexProvider.cpp
            guid.cpp ReaderThread.cpp FlexProviderFactory.cpp helpers.cpp
            ../../util.cpp resources.rc)
target_link_libraries(FlexVDICredentialProvider ShLwApi Secur32 Uuid)
set_target_properties(FlexVDICredentialProvider PROPERTIES LINK_FLAGS
    "-Wl,-enable-stdcall-fixup ${DEF_FILE} -static-libgcc -static-libstdc++"
    PREFIX "")
