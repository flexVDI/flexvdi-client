include_directories(${PROJECT_SOURCE_DIR})
add_executable(get_printer_ppds get_printer_ppds.c)
target_link_libraries(get_printer_ppds flexvdi-client ${CLIENT_LIBRARIES} m z pthread)

add_executable(test_terminal_id test_terminal_id.c)
target_link_libraries(test_terminal_id flexvdi-client ${CLIENT_LIBRARIES} m z pthread)
add_test(terminal_id test_terminal_id)

add_executable(test_client_log test_client_log.c)
target_link_libraries(test_client_log flexvdi-client ${CLIENT_LIBRARIES} m z pthread)
add_test(client_log test_client_log)

add_executable(test_client_request test_client_request.c)
target_link_libraries(test_client_request flexvdi-client ${CLIENT_LIBRARIES} m z pthread)
add_test(client_request test_client_request)
