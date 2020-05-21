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

#ifndef _CLIENT_REQUEST_H
#define _CLIENT_REQUEST_H

#include <glib-object.h>

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
    const gchar * post_data, const gchar * loggable_post_data, ClientRequestCallback cb, gpointer user_data);
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
