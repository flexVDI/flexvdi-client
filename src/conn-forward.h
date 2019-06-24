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

typedef struct ConnForwarder ConnForwarder;

/*
 * Callback to send commands to the vdagent.
 */
typedef void (* conn_forwarder_send_command_cb)(
    guint32 command, const guint8 * data, guint32 data_size, gpointer user_data);

ConnForwarder * conn_forwarder_new(conn_forwarder_send_command_cb cb, gpointer user_data);

void conn_forwarder_delete(ConnForwarder * cf);

void conn_forwarder_agent_disconnected(ConnForwarder * cf);

/*
 * Associate a local port with a remote endpoint.
 * 
 * @local: String representation of the redirection. One of:
 *         [bind_address:]local_port:remote_address:remote_port
 *         (Under development:)
 *         [bind_address:]local_port:remote_socket
 *         UDP:[bind_address:]local_port:remote_address:remote_port
 *         local_socket:remote_address:remote_port
 *         local_socket:remote_socket
 *         [bind_address:]port
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean conn_forwarder_associate_local(ConnForwarder * cf, const gchar * local);

/*
 * Associate a remote port with a local port.
 * 
 * @remote: String representation of the redirection. One of:
 *          [bind_address:]remote_port:local_address:local_port
 *          (Under development:)
 *          [bind_address:]remote_port:local_socket
 *          UDP:[bind_address:]remote_port:local_address:local_port
 *          remote_socket:local_address:local_port
 *          remote_socket:local_socket
 *          [bind_address:]port
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean conn_forwarder_associate_remote(ConnForwarder * cf, const gchar * remote);

/*
 * Handle a message received from the agent.
 */
void conn_forwarder_handle_message(ConnForwarder * cf, guint32 command, gpointer msg);

#endif /* __CONN_FORWARD_H */
