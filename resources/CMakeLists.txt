set(resources_c "${CMAKE_CURRENT_BINARY_DIR}/resources.c")
add_custom_command(OUTPUT ${resources_c}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   COMMAND glib-compile-resources
                   ARGS --target=${resources_c} --generate-source flexvdi-client.gresource.xml
                   DEPENDS
                        flexvdi-client.gresource.xml
                        window.ui
                        spice-win.ui
                        style.css
                        images/icon.png
                        images/logo-flexVDI-toolbar.png
                        images/button-settings.png
                        images/logo-client.png
                  )

add_library(resource_objects OBJECT ${resources_c})
set_target_properties(resource_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)
