/*
    Copyright (C) 2014-2019 Flexible Software Solutions S.L.U.

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

#ifndef __CONN_FORWARD_H
#define __CONN_FORWARD_H

#include <glib.h>
#include "flexvdi-port.h"

typedef struct ConnForwarder ConnForwarder;

ConnForwarder * conn_forwarder_new(FlexvdiPort * guest_agent_port);

void conn_forwarder_delete(ConnForwarder * cf);

/*
 * Set redirections.
 * 
 * @local: List of string representations of the local redirections. Format:
 *         [bind_address:]local_port:remote_address:remote_port
 *         (Under development:)
 *         [bind_address:]local_port:remote_socket
 *         UDP:[bind_address:]local_port:remote_address:remote_port
 *         local_socket:remote_address:remote_port
 *         local_socket:remote_socket
 *         [bind_address:]port
 *
 * @remote: List of string representations of the remote redirections. Format:
 *          [bind_address:]remote_port:local_address:local_port
 *          (Under development:)
 *          [bind_address:]remote_port:local_socket
 *          UDP:[bind_address:]remote_port:local_address:local_port
 *          remote_socket:local_address:local_port
 *          remote_socket:local_socket
 *          [bind_address:]port
 */
void conn_forwarder_set_redirections(ConnForwarder * cf, gchar ** local, gchar ** remote);

#endif /* __CONN_FORWARD_H */
