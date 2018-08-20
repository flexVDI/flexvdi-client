/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#ifndef _CLIENT_CONN_H
#define _CLIENT_CONN_H

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <spice-client-gtk.h>

#include "configuration.h"


typedef enum {
    CLIENT_CONN_DISCONNECT_NO_ERROR = 0,
    CLIENT_CONN_DISCONNECT_CONN_ERROR,
    CLIENT_CONN_ISCONNECT_IO_ERROR,
    CLIENT_CONN_DISCONNECT_AUTH_ERROR,
} ClientConnDisconnectReason;

#define CLIENT_CONN_TYPE (client_conn_get_type())
G_DECLARE_FINAL_TYPE(ClientConn, client_conn, CLIENT, CONN, GObject)

ClientConn * client_conn_new(ClientConf * conf, JsonObject * params);
ClientConn * client_conn_new_with_uri(ClientConf * conf, const char * uri);
void client_conn_connect(ClientConn * conn);
void client_conn_disconnect(ClientConn * conn, ClientConnDisconnectReason reason);
SpiceSession * client_conn_get_session(ClientConn * conn);
SpiceGtkSession * client_conn_get_gtk_session(ClientConn * conn);
SpiceMainChannel * client_conn_get_main_channel(ClientConn * conn);


#endif /* _CLIENT_CONN_H */
