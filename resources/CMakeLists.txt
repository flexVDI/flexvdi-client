set(resources_c "${CMAKE_CURRENT_BINARY_DIR}/resources.c")
add_custom_command(OUTPUT ${resources_c}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   COMMAND glib-compile-resources
                   ARGS --target=${resources_c} --generate-source flexvdi-client.gresource.xml
                   DEPENDS
                        flexvdi-client.gresource.xml
                        window.ui
                        spice-win.ui
                        keys-menu-linux.ui
                        keys-menu-windows.ui
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
                        icons/16x16/actions/window-restore.png
                        icons/32x32/actions/close-flexvdi.png
                        icons/32x32/actions/copy-flexvdi.png
                        icons/32x32/actions/paste-flexvdi.png
                        icons/32x32/actions/fullscreen-flexvdi.png
                        icons/32x32/actions/restore-flexvdi.png
                        icons/32x32/actions/minimize-flexvdi.png
                        icons/32x32/actions/system-poweroff-flexvdi.png
                        icons/32x32/actions/system-reboot-flexvdi.png
                        icons/32x32/actions/system-shutdown-flexvdi.png
                        icons/32x32/actions/keyboard-flexvdi.png
                        icons/32x32/actions/usb-flexvdi.png
                        icons/32x32/actions/printer-flexvdi.png
                        icons/32x32/actions/about-flexvdi.png
                        icons/24x24/actions/about-flexvdi-white.png
                  )

add_library(resource_objects OBJECT ${resources_c})
set_target_properties(resource_objects PROPERTIES POSITION_INDEPENDENT_CODE 1)
