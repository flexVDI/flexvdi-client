/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#ifndef _CLIENT_REQUEST_H
#define _CLIENT_REQUEST_H

#include <glib-object.h>
#include <json-glib/json-glib.h>

#include "configuration.h"


#define CLIENT_REQUEST_TYPE (client_request_get_type())
G_DECLARE_FINAL_TYPE(ClientRequest, client_request, CLIENT, REQUEST, GObject)

typedef void (* ClientRequestCallback)(ClientRequest *, gpointer);
ClientRequest * client_request_new(ClientConf * conf, const gchar * path,
    ClientRequestCallback cb, gpointer user_data);
ClientRequest * client_request_new_with_data(ClientConf * conf, const gchar * path,
    const gchar * post_data, ClientRequestCallback cb, gpointer user_data);
void client_request_cancel(ClientRequest * req);
JsonNode * client_request_get_result(ClientRequest * req, GError ** error);


#endif /* _CLIENT_REQUEST_H */
