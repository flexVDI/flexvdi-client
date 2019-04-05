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

#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <spice-client.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>


typedef enum _WindowEdge {
    WINDOW_EDGE_UP = 0,
    WINDOW_EDGE_DOWN,
    WINDOW_EDGE_LEFT,
    WINDOW_EDGE_RIGHT
} WindowEdge;


/*
 * ClientConf
 *
 * A configuration manager for the application. Configuration parameters are first
 * read from a file, then overriden by any option passed through command line.
 * Every option can be set both in the configuration file and from command line.
 * Some options can also be saved back to the configuration file, like the server
 * IP and port, username or shared printers.
 */
#define CLIENT_CONF_TYPE (client_conf_get_type())
G_DECLARE_FINAL_TYPE(ClientConf, client_conf, CLIENT, CONF, GObject)

/*
 * client_conf_new
 *
 * Create a new ClientConf object. Only one should be used throughout the app.
 * As a side effect, the configuration file is read.
 */
ClientConf * client_conf_new(void);

/*
 * client_conf_set_original_arguments
 *
 * Pass the original argument list so that we can know whan arguments were
 * specified in the command line, to keep them from being overwritten by the
 * configuration file
 */
void client_conf_set_original_arguments(ClientConf * conf, gchar ** arguments);

/*
 * client_conf_get_options_from_response
 *
 * Set some parameters as provided by the response from the Manager, like disabled
 * features or the inactivity timeout.
 */
void client_conf_get_options_from_response(ClientConf * conf, JsonObject * params);

/*
 * client_conf_set_application_options
 *
 * Set the option groups so that command line options can be parsed.
 */
void client_conf_set_application_options(ClientConf * conf, GApplication * app);

/*
 * client_conf_set_session_options
 *
 * Transfer session options to a SpiceSession object, like connection parameters.
 */
void client_conf_set_session_options(ClientConf * conf, SpiceSession * session);

/*
 * client_conf_set_gtk_session_options
 *
 * Transfer session options to a SpiceGtkSession object. The object is not typed
 * to avoid depending on libspice-client-gtk.
 */
void client_conf_set_gtk_session_options(ClientConf * conf, GObject * gtk_session);

/*
 * client_conf_set_display_options
 *
 * Transfer display options to a SpiceDisplay object, like pointer grab behaviour.
 */
void client_conf_set_display_options(ClientConf * conf, GObject * display);

/*
 * client_conf_had_file
 *
 * Return whether a config file was read.
 */
gboolean client_conf_had_file(ClientConf * conf);

/*
 * Getters for some options, those needed directly by the application. Some options
 * that are set through the previous two functions do not have a corresponding getter.
 */
const gchar * client_conf_get_host(ClientConf * conf);
const gchar * client_conf_get_port(ClientConf * conf);
const gchar * client_conf_get_username(ClientConf * conf);
const gchar * client_conf_get_password(ClientConf * conf);
const gchar * client_conf_get_desktop(ClientConf * conf);
const gchar * client_conf_get_uri(ClientConf * conf);
const gchar * client_conf_get_proxy_uri(ClientConf * conf);
gchar * client_conf_get_connection_uri(ClientConf * conf, const gchar * path);
gboolean client_conf_get_fullscreen(ClientConf * conf);
gchar ** client_conf_get_serial_params(ClientConf * conf);
gboolean client_conf_get_disable_printing(ClientConf * conf);
const gchar * client_conf_get_terminal_id(ClientConf * conf);
gboolean client_conf_get_disable_copy_from_guest(ClientConf * conf);
gboolean client_conf_get_disable_paste_to_guest(ClientConf * conf);
gboolean client_conf_get_disable_power_actions(ClientConf * conf);
gboolean client_conf_is_printer_shared(ClientConf * conf, const gchar * printer);
gchar * client_conf_get_grab_sequence(ClientConf * conf);
gint client_conf_get_inactivity_timeout(ClientConf * conf);
gboolean client_conf_get_auto_clipboard(ClientConf * conf);
SoupSession * client_conf_get_soup_session(ClientConf * conf);
WindowEdge client_conf_get_toolbar_edge(ClientConf * conf);

/*
 * Setters for those options that can be saved to disk.
 */
void client_conf_set_host(ClientConf * conf, const gchar * host);
void client_conf_set_port(ClientConf * conf, const gchar * port);
void client_conf_set_username(ClientConf * conf, const gchar * username);
void client_conf_set_uri(ClientConf * conf, const gchar * uri);
void client_conf_set_fullscreen(ClientConf * conf, gboolean fs);
void client_conf_set_proxy_uri(ClientConf * conf, const gchar * proxy_uri);
void client_conf_share_printer(ClientConf * conf, const gchar * printer, gboolean share);
void client_conf_set_window_size(ClientConf * conf, gint id,
    int width, int height, gboolean maximized, int monitor);
gboolean client_conf_get_window_size(ClientConf * conf, gint id,
    int * width, int * height, gboolean * maximized, int * monitor);

/*
 * client_conf_save
 *
 * Save the configuration to file.
 */
void client_conf_save(ClientConf * conf);

#endif /* _CONFIGURATION_H */
