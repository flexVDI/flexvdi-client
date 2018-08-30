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
                        icons/16x16/actions/user.png
                        icons/16x16/actions/lock.png
                        icons/16x16/actions/window-close.png
                        icons/16x16/actions/window-minimize.png
                        icons/16x16/actions/window-maximize.png
                        icons/24x24/actions/window-close-cus.png
                        icons/24x24/actions/edit-copy-cus.png
                        icons/24x24/actions/edit-paste-cus.png
                        icons/24x24/actions/view-fullscreen-cus.png
                        icons/24x24/actions/view-restore-cus.png
                        icons/24x24/actions/go-bottom-cus.png
                        icons/24x24/actions/system-poweroff-cus.png
                        icons/24x24/actions/system-reboot-cus.png
                        icons/24x24/actions/system-shutdown-cus.png
                        icons/24x24/actions/input-keyboard-cus.png
                        icons/24x24/actions/drive-removable-usb-cus.png
                        icons/24x24/actions/printer-cus.png
                  )

add_library(resource_objects OBJECT ${resources_c})
set_target_properties(resource_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)
