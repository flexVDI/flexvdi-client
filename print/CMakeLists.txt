if (WIN32)
    set(agent_sources ${agent_sources} ${CMAKE_CURRENT_SOURCE_DIR}/PrintManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PrintManager-win32.cpp PARENT_SCOPE)
    add_subdirectory(windriver)
else (WIN32)
    set(agent_sources ${agent_sources} ${CMAKE_CURRENT_SOURCE_DIR}/PrintManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PrintManager-cups.cpp PARENT_SCOPE)
    add_subdirectory(cupsbackend)
endif(WIN32)
