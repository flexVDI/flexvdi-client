/*
    Copyright (C) 2014-2018 Flexible Software Solutions S.L.U.

    This file is part of flexVDI Client.

    flexVDI Client is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    flexVDI Client is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flexVDI Client. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _CLIENT_WIN_H
#define _CLIENT_WIN_H

#include <gtk/gtk.h>
#include "client-app.h"
#include "configuration.h"


/*
 * ClientAppWindow
 *
 * Main window class. Presents a settings page, a login page and a desktop selection page.
 * It is the user interface with the flexVDI Manager until the Spice connection is established.
 */
#define CLIENT_APP_WINDOW_TYPE (client_app_window_get_type ())
G_DECLARE_FINAL_TYPE(ClientAppWindow, client_app_window, CLIENT, APP_WINDOW, GtkApplicationWindow)

enum {
    SETTINGS_BUTTON = 0,
    ABOUT_BUTTON,
    SAVE_BUTTON,
    DISCARD_BUTTON,
    LOGIN_BUTTON,
    BACK_BUTTON
} ClientAppWindowButtons;

/*
 * client_app_window_new
 *
 * Create a new main window associated with an application.
 */
ClientAppWindow * client_app_window_new(ClientApp * app);

/*
 * client_app_window_load_config
 *
 * Settings are loaded from the configuration object into the corresponding
 * text boxes in the main window.
 */
void client_app_window_load_config(ClientAppWindow * win, ClientConf * conf);

/*
 * client_app_window_save_config
 *
 * Settings are saved into the configuration object from the corresponding
 * text boxes in the main window.
 */
void client_app_window_save_config(ClientAppWindow * win, ClientConf * conf);

/*
 * client_app_window_set_info
 *
 * Set the info text label.
 */
void client_app_window_set_info(ClientAppWindow * win, const gchar * text);

/*
 * client_app_window_status
 *
 * Show a message in the status area. This area slides down when a new
 * message is set.
 */
void client_app_window_status(ClientAppWindow * win, const gchar * text);

/*
 * client_app_window_error
 *
 * Show a message in the status area, with the error class.
 */
void client_app_window_error(ClientAppWindow * win, const gchar * text);

/*
 * client_app_window_hide_status
 *
 * Hide the status area.
 */
void client_app_window_hide_status(ClientAppWindow * win);

/*
 * client_app_window_set_central_widget
 *
 * Set the page that is show in the central widget. Name is one of "settings", "login" or "desktops".
 */
void client_app_window_set_central_widget(ClientAppWindow * win, const gchar * name);

/*
 * client_app_window_set_central_widget_sensitive
 *
 * Set whether the central widget accepts user input or not. Usually, the central widget
 * is disabled while a request is in process.
 */
void client_app_window_set_central_widget_sensitive(ClientAppWindow * win, gboolean sensitive);

/*
 * client_app_window_get_username
 *
 * Get username as entered by the user in the text box.
 */
const gchar * client_app_window_get_username(ClientAppWindow * win);

/*
 * client_app_window_get_password
 *
 * Get password as entered by the user in the text box.
 */
const gchar * client_app_window_get_password(ClientAppWindow * win);

/*
 * client_app_window_set_desktops
 *
 * Set the list of desktops the user can select.
 */
void client_app_window_set_desktops(ClientAppWindow * win, GList * desktop_names,
                                    const gchar * desktop);

/*
 * client_app_window_get_desktop
 *
 * Get the currently selected desktop.
 */
gchar * client_app_window_get_desktop(ClientAppWindow * win);

/*
 * client_app_window_hide_config_button
 *
 * Hide the configuration button.
 */
void client_app_window_hide_config_button(ClientAppWindow * win);


#endif /* _CLIENT_WIN_H */
