/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <glib-object.h>


#define CLIENT_CONF_TYPE (client_conf_get_type())
G_DECLARE_FINAL_TYPE(ClientConf, client_conf, CLIENT, CONF, GObject)

ClientConf * client_conf_new(void);
GOptionEntry * client_conf_get_cmdline_entries(ClientConf * conf);
gboolean client_conf_show_version(ClientConf * conf);
const gchar * client_conf_get_host(ClientConf * conf);
gint client_conf_get_port(ClientConf * conf);
gchar * client_conf_get_connection_uri(ClientConf * conf, const gchar * path);
gboolean client_conf_get_fullscreen(ClientConf * conf);
const gchar ** client_conf_get_serial_params(ClientConf * conf);
gboolean client_conf_get_disable_printing(ClientConf * conf);
const gchar * client_conf_get_terminal_id(ClientConf * conf);


#endif /* _CONFIGURATION_H */
