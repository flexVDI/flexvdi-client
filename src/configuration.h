/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <spice-client-gtk.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>


#define CLIENT_CONF_TYPE (client_conf_get_type())
G_DECLARE_FINAL_TYPE(ClientConf, client_conf, CLIENT, CONF, GObject)

ClientConf * client_conf_new(void);
void client_conf_get_options_from_response(ClientConf * conf, JsonObject * params);
void client_conf_set_application_options(ClientConf * conf, GApplication * app);
void client_conf_set_session_options(ClientConf * conf, SpiceSession * session);
void client_conf_set_display_options(ClientConf * conf, SpiceDisplay * display);

gboolean client_conf_show_version(ClientConf * conf);
const gchar * client_conf_get_host(ClientConf * conf);
const gchar * client_conf_get_port(ClientConf * conf);
const gchar * client_conf_get_username(ClientConf * conf);
const gchar * client_conf_get_password(ClientConf * conf);
const gchar * client_conf_get_desktop(ClientConf * conf);
const gchar * client_conf_get_uri(ClientConf * conf);
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

void client_conf_set_host(ClientConf * conf, const gchar * host);
void client_conf_set_port(ClientConf * conf, const gchar * port);
void client_conf_set_username(ClientConf * conf, const gchar * username);
void client_conf_set_uri(ClientConf * conf, const gchar * uri);
void client_conf_set_fullscreen(ClientConf * conf, gboolean fs);
void client_conf_share_printer(ClientConf * conf, const gchar * printer, gboolean share);
void client_conf_save(ClientConf * conf);

#endif /* _CONFIGURATION_H */
