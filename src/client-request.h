/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#ifndef _CLIENT_REQUEST_H
#define _CLIENT_REQUEST_H

#include <glib-object.h>
#include <json-glib/json-glib.h>

#include "configuration.h"


/*
 * ClientRequest
 * 
 * An asynchronous HTTPS request to the flexVDI Manager, with or without data.
 */
#define CLIENT_REQUEST_TYPE (client_request_get_type())
G_DECLARE_FINAL_TYPE(ClientRequest, client_request, CLIENT, REQUEST, GObject)

/*
 * Request callback to handle the response
 */
typedef void (* ClientRequestCallback)(ClientRequest *, gpointer);

/*
 * client_request_new
 * 
 * Create a new GET request. cb is called when the response arrives.
 */
ClientRequest * client_request_new(ClientConf * conf, const gchar * path,
    ClientRequestCallback cb, gpointer user_data);

/*
 * client_request_new_with_data
 * 
 * Create a new POST request, with data.
 */
ClientRequest * client_request_new_with_data(ClientConf * conf, const gchar * path,
    const gchar * post_data, ClientRequestCallback cb, gpointer user_data);

/*
 * client_request_cancel
 * 
 * Cancel an ongoing request. The callback is not called.
 */
void client_request_cancel(ClientRequest * req);

/*
 * client_request_get_result
 * 
 * Get the response as a JsonNode object, or an error in case of failure.
 */
JsonNode * client_request_get_result(ClientRequest * req, GError ** error);


#endif /* _CLIENT_REQUEST_H */
