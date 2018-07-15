/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <spice-client-gtk.h>
#include <glib-object.h>


#define CLIENT_CONF_TYPE (client_conf_get_type())
G_DECLARE_FINAL_TYPE(ClientConf, client_conf, CLIENT, CONF, GObject)

ClientConf * client_conf_new(void);
void client_conf_set_application_options(ClientConf * conf, GApplication * app);
void client_conf_set_session_options(ClientConf * conf, SpiceSession * session);

gboolean client_conf_show_version(ClientConf * conf);
const gchar * client_conf_get_host(ClientConf * conf);
const gchar * client_conf_get_port(ClientConf * conf);
const gchar * client_conf_get_username(ClientConf * conf);
const gchar * client_conf_get_password(ClientConf * conf);
const gchar * client_conf_get_desktop(ClientConf * conf);
gchar * client_conf_get_connection_uri(ClientConf * conf, const gchar * path);
gboolean client_conf_get_fullscreen(ClientConf * conf);
gchar ** client_conf_get_serial_params(ClientConf * conf);
gboolean client_conf_get_disable_printing(ClientConf * conf);
const gchar * client_conf_get_terminal_id(ClientConf * conf);

void client_conf_set_host(ClientConf * conf, const gchar * host);
void client_conf_set_port(ClientConf * conf, const gchar * port);
void client_conf_set_fullscreen(ClientConf * conf, gboolean fs);
void client_conf_save(ClientConf * conf);

#endif /* _CONFIGURATION_H */
