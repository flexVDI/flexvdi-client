/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#ifndef _SPICE_CONN_H
#define _SPICE_CONN_H

#include <glib-object.h>
#include <json-glib/json-glib.h>

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
void client_conn_connect(ClientConn * conn);
void client_conn_disconnect(ClientConn * conn, ClientConnDisconnectReason reason);


#endif /* _SPICE_CONN_H */
