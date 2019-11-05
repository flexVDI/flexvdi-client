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

#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <flexdp.h>
#include "conn-forward.h"

struct _ConnForwarder {
    GObject parent;
    FlexvdiPort * port;
    gchar ** local;
    gchar ** remote;
    GHashTable * remote_assocs;
    GHashTable * connections;
    GSocketListener * listener;
    GCancellable * listener_cancellable;
};

G_DEFINE_TYPE(ConnForwarder, conn_forwarder, G_TYPE_OBJECT);

#define WINDOW_SIZE 10*1024*1024


#define CONNECTION_TYPE (connection_get_type())
G_DECLARE_FINAL_TYPE(Connection, connection, FLEXVDI, CONNECTION, GObject)

typedef struct _Connection {
    GObject parent;
    GSocketClient * socket;
    GSocketConnection * conn;
    GCancellable * cancellable;
    GQueue * write_buffer;
    uint8_t * read_buffer;
    guint32 data_sent, data_received, ack_interval;
    gboolean connecting;
    ConnForwarder * cf;
    guint32 id;
} Connection;

G_DEFINE_TYPE(Connection, connection, G_TYPE_OBJECT);


static void connecction_dispose(GObject * gobject);
static void connection_finalize(GObject * gobject);

static void connection_class_init(ConnectionClass * class) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(class);
    gobject_class->dispose = connecction_dispose;
    gobject_class->finalize = connection_finalize;
}


static void connection_init(Connection * conn) {
    conn->cancellable = g_cancellable_new();
    conn->connecting = TRUE;
    conn->write_buffer = g_queue_new();
}


static void connecction_dispose(GObject * obj) {
    Connection * conn = FLEXVDI_CONNECTION(obj);
    g_clear_object(&conn->cancellable);
    if (conn->conn)
        g_io_stream_close((GIOStream *)conn->conn, NULL, NULL);
    g_clear_object(&conn->conn);
    g_clear_object(&conn->socket);
    G_OBJECT_CLASS(connection_parent_class)->dispose(obj);
}


static void connection_finalize(GObject * gobject) {
    Connection * conn = FLEXVDI_CONNECTION(gobject);
    g_debug("Closing connection %u", conn->id);
    g_queue_free_full(conn->write_buffer, (GDestroyNotify)g_bytes_unref);
    if (conn->read_buffer)
        flexvdi_port_delete_msg_buffer(conn->read_buffer);
    G_OBJECT_CLASS(connection_parent_class)->finalize(gobject);
}


static Connection * connection_new(ConnForwarder * cf, int id, guint32 ack_int) {
    Connection * conn = g_object_new(CONNECTION_TYPE, NULL);
    conn->id = id;
    conn->cf = cf;
    conn->ack_interval = ack_int;
    return conn;
}


static Connection * connection_new_with_socket(ConnForwarder * cf, int id, guint32 ack_int) {
    Connection * conn = connection_new(cf, id, ack_int);
    conn->socket = g_socket_client_new();
    return conn;
}


static Connection * connection_new_with_open_socket(ConnForwarder * cf, int id, guint32 ack_int,
                                                    GSocketConnection * open_conn) {
    Connection * conn = connection_new(cf, id, ack_int);
    conn->conn = open_conn;
    return conn;
}


static void connection_close(Connection * conn) {
    g_debug("Start closing connection %u", conn->id);
    if (!g_cancellable_is_cancelled(conn->cancellable))
        g_cancellable_cancel(conn->cancellable);
    if (!g_hash_table_remove(conn->cf->connections, GUINT_TO_POINTER(conn->id)))
        g_debug("Connection not found in hash table with id %p???", GUINT_TO_POINTER(conn->id));
}


#define ADDRESS_PORT_TYPE (address_port_get_type())
G_DECLARE_FINAL_TYPE(AddressPort, address_port, ADDRESS, PORT, GObject)

typedef struct _AddressPort {
    GObject parent_instance;
    guint16 port;
    gchar * address;
} AddressPort;

G_DEFINE_TYPE(AddressPort, address_port, G_TYPE_OBJECT);


static void address_port_finalize(GObject * gobject);

static void address_port_class_init(AddressPortClass * class) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(class);
    gobject_class->finalize = address_port_finalize;
}


static void address_port_init(AddressPort * addr) {}


static void address_port_finalize(GObject * gobject) {
    AddressPort * addr = ADDRESS_PORT(gobject);
    g_free(addr->address);
    G_OBJECT_CLASS(address_port_parent_class)->finalize(gobject);
}


static gpointer address_port_new(guint16 port, const char * address) {
    AddressPort * addr = g_object_new(ADDRESS_PORT_TYPE, NULL);
    addr->port = port;
    addr->address = g_strdup(address);
    return addr;
}


static void conn_forwarder_dispose(GObject * obj);
static void conn_forwarder_finalize(GObject * obj);

static void conn_forwarder_class_init(ConnForwarderClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->dispose = conn_forwarder_dispose;
    object_class->finalize = conn_forwarder_finalize;
}


static void conn_forwarder_init(ConnForwarder * cf) {
    cf->remote_assocs = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                              NULL, g_object_unref);
    cf->connections = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                            NULL, g_object_unref);
    cf->listener = g_socket_listener_new();
    cf->listener_cancellable = g_cancellable_new();
}


static void conn_forwarder_dispose(GObject * obj) {
    ConnForwarder * cf = CONN_FORWARDER(obj);
    g_clear_object(&cf->port);
    g_clear_object(&cf->listener_cancellable);
    g_clear_object(&cf->listener);
    G_OBJECT_CLASS(conn_forwarder_parent_class)->dispose(obj);
}


static void conn_forwarder_finalize(GObject * obj) {
    ConnForwarder * cf = CONN_FORWARDER(obj);
    g_hash_table_destroy(cf->remote_assocs);
    g_hash_table_destroy(cf->connections);
    G_OBJECT_CLASS(conn_forwarder_parent_class)->finalize(obj);
}


static void guest_agent_connected(FlexvdiPort * port, gboolean connected, ConnForwarder * cf);
static gboolean conn_forwarder_handle_message(FlexvdiPort * port, int type, gpointer msg, gpointer data);

ConnForwarder * conn_forwarder_new(FlexvdiPort * guest_agent_port) {
    ConnForwarder * cf = g_object_new(CONN_FORWARDER_TYPE, NULL);
    cf->port = g_object_ref(guest_agent_port);
    g_signal_connect(guest_agent_port, "agent-connected", G_CALLBACK(guest_agent_connected), cf);
    g_signal_connect(guest_agent_port, "message", G_CALLBACK(conn_forwarder_handle_message), cf);
    g_debug("Created new port forwarder");
    return cf;
}


void conn_forwarder_set_redirections(ConnForwarder * cf, gchar ** local, gchar ** remote) {
    cf->local = local;
    cf->remote = remote;
}


static gboolean conn_forwarder_disassociate_remote(ConnForwarder * cf, guint16 rport) {
    if (!g_hash_table_remove(cf->remote_assocs, GUINT_TO_POINTER(rport))) {
        g_warning("Remote port %d is not associated with a local port.", rport);
        return FALSE;
    } else {
        g_debug("Disassociate remote port %d", rport);
        uint8_t * buf = flexvdi_port_get_msg_buffer(sizeof(FlexVDIForwardShutdownMsg));
        FlexVDIForwardShutdownMsg * shtMsg = (FlexVDIForwardShutdownMsg *)buf;
        shtMsg->listenId = rport;
        flexvdi_port_send_msg(cf->port, FLEXVDI_FWDSHUTDOWN, buf);
        return TRUE;
    }
}


static gboolean tokenize_redirection(gchar * redir, gchar ** bind_address, gchar ** port,
                                     gchar ** host, gchar ** host_port) {
    if ((* bind_address = strtok(redir, ":")) &&
            (* port = strtok(NULL, ":")) &&
            (* host = strtok(NULL, ":"))) {
        if (!(* host_port = strtok(NULL, ":"))) {
            // bind_address was not provided
            * host_port = * host;
            * host = * port;
            * port = * bind_address;
            * bind_address = NULL;
        }
        return TRUE;
    } else
        return FALSE;
}


static gboolean conn_forwarder_associate_remote(ConnForwarder * cf, const gchar * remote) {
    gchar * bind_address, * guest_port, * host, * host_port;
    g_autofree gchar * remote_copy = g_strdup(remote);
    if (!tokenize_redirection(remote_copy, &bind_address, &guest_port, &host, &host_port)) {
        g_warning("Unknown redirection '%s'", remote);
        return FALSE;
    }

    guint16 rport = atoi(guest_port), lport = atoi(host_port);
    g_debug("Associate guest %s, port %d -> %s port %d", bind_address, rport, host, lport);
    if (g_hash_table_lookup(cf->remote_assocs, GUINT_TO_POINTER(rport))) {
        conn_forwarder_disassociate_remote(cf, rport);
    }
    g_hash_table_insert(cf->remote_assocs, GUINT_TO_POINTER(rport),
                        address_port_new(lport, host));

    if (!bind_address) {
        bind_address = "localhost";
    }
    int addr_len = strlen(bind_address);
    int msg_len = sizeof(FlexVDIForwardListenMsg) + addr_len + 1;
    uint8_t * buf = flexvdi_port_get_msg_buffer(msg_len);
    FlexVDIForwardListenMsg * msg = (FlexVDIForwardListenMsg *)buf;
    msg->id = msg->port = rport;
    msg->proto = FLEXVDI_FWDPROTO_TCP;
    msg->addressLength = addr_len;
    strcpy(msg->address, bind_address);
    flexvdi_port_send_msg(cf->port, FLEXVDI_FWDLISTEN, buf);
    return TRUE;
}


static void listener_accept_callback(GObject * source_object, GAsyncResult * res,
                                     gpointer user_data);

static gboolean conn_forwarder_associate_local(ConnForwarder * cf, const gchar * local) {
    gchar * bind_address, * local_port, * host, * host_port;
    g_autofree gchar * local_copy = g_strdup(local);
    if (!tokenize_redirection(local_copy, &bind_address, &local_port, &host, &host_port)) {
        g_warning("Unknown redirection '%s'", local);
        return FALSE;
    }

    guint16 lport = atoi(local_port), rport = atoi(host_port);
    // Listen and wait for a connection
    gboolean res;
    if (bind_address) {
        GInetAddress * inet_address = g_inet_address_new_from_string(bind_address);
        GSocketAddress * socket_address = g_inet_socket_address_new(inet_address, lport);
        res = g_socket_listener_add_address(cf->listener, socket_address,
                                            G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT,
                                            address_port_new(rport, host), NULL, NULL);
    } else {
        res = g_socket_listener_add_inet_port(cf->listener, lport,
                                              address_port_new(rport, host), NULL);
    }
    g_cancellable_cancel(cf->listener_cancellable);
    g_object_unref(cf->listener_cancellable);
    cf->listener_cancellable = g_cancellable_new();
    g_socket_listener_accept_async(cf->listener, cf->listener_cancellable,
                                   listener_accept_callback, cf);
    return res;
}


static void guest_agent_connected(FlexvdiPort * port, gboolean connected, ConnForwarder * cf) {
    if (connected && flexvdi_port_agent_supports_capability(port, FLEXVDI_CAP_FORWARD)) {
        gchar ** it;
        for (it = cf->local; it != NULL && *it != NULL; ++it)
            conn_forwarder_associate_local(cf, *it);
        for (it = cf->remote; it != NULL && *it != NULL; ++it)
            conn_forwarder_associate_remote(cf, *it);
    } else if (!connected) {
        g_debug("Agent disconnected, close all connections");
        g_hash_table_remove_all(cf->remote_assocs);
        g_hash_table_remove_all(cf->connections);
    }
}


static guint32 generate_connection_id(void) {
    static guint32 seq = 0;
    return --seq;
}

static void listener_accept_callback(GObject * source_object, GAsyncResult * res,
                                     gpointer user_data) {
    ConnForwarder * cf = CONN_FORWARDER(user_data);
    GError * error = NULL;
    GSocketConnection * sc = g_socket_listener_accept_finish(cf->listener, res,
                                                            &source_object, &error);
    if (error) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning("Could not accept connection");
    } else {
        GInetSocketAddress * local_address =
                (GInetSocketAddress *)g_socket_connection_get_local_address(sc, NULL);
        guint16 port = g_inet_socket_address_get_port(local_address);
        AddressPort * host = ADDRESS_PORT(source_object);
        g_debug("Accepted connection on port %d to %s:%d",
                    port, host->address, host->port);

        Connection * conn = connection_new_with_open_socket(cf, generate_connection_id(),
                                                            WINDOW_SIZE / 2, sc);
        if (conn) {
            int addr_len = strlen(host->address);
            int msg_len = sizeof(FlexVDIForwardConnectMsg) + addr_len + 1;
            uint8_t * buf = flexvdi_port_get_msg_buffer(msg_len);
            FlexVDIForwardConnectMsg * msg = (FlexVDIForwardConnectMsg *)buf;
            msg->id = conn->id;
            msg->winSize = conn->ack_interval * 2;
            msg->proto = FLEXVDI_FWDPROTO_TCP;
            msg->port = host->port;
            msg->addressLength = addr_len;
            strcpy(msg->address, host->address);
            flexvdi_port_send_msg(cf->port, FLEXVDI_FWDCONNECT, buf);
            g_hash_table_insert(cf->connections, GUINT_TO_POINTER(conn->id), conn);
            g_debug("Inserted connection in table with id %p", GUINT_TO_POINTER(conn->id));
        }

        g_socket_listener_accept_async(cf->listener, cf->listener_cancellable,
                                       listener_accept_callback, cf);
    }
}

static void connection_read_callback(GObject * source_object, GAsyncResult * res,
                                     gpointer user_data);

#define MAX_MSG_SIZE FLEXVDI_MAX_MESSAGE_LENGTH - sizeof(FlexVDIMessageHeader)

static void program_read(Connection * conn) {
    GInputStream * stream = g_io_stream_get_input_stream((GIOStream *)conn->conn);
    conn->read_buffer = flexvdi_port_get_msg_buffer(MAX_MSG_SIZE);
    FlexVDIForwardDataMsg * msg = (FlexVDIForwardDataMsg *)conn->read_buffer;
    g_input_stream_read_async(stream, msg->data, MAX_MSG_SIZE - sizeof(*msg), G_PRIORITY_DEFAULT,
                              conn->cancellable, connection_read_callback, conn);
}


static void close_agent_connection(ConnForwarder * cf, int id) {
    uint8_t * buf = flexvdi_port_get_msg_buffer(sizeof(FlexVDIForwardCloseMsg));
    FlexVDIForwardCloseMsg * closeMsg = (FlexVDIForwardCloseMsg *)buf;
    closeMsg->id = id;
    flexvdi_port_send_msg(cf->port, FLEXVDI_FWDCLOSE, buf);
}


static void connection_close_unref(Connection * conn) {
    close_agent_connection(conn->cf, conn->id);
    connection_close(conn);
    g_object_unref(conn);
}


static void connection_read_callback(GObject * source_object, GAsyncResult * res,
                                     gpointer user_data) {
    Connection * conn = (Connection *)user_data;
    ConnForwarder * cf = conn->cf;
    GError * error = NULL;
    GInputStream * stream = (GInputStream *)source_object;
    gssize bytes = g_input_stream_read_finish(stream, res, &error);
    FlexVDIForwardDataMsg * msg = (FlexVDIForwardDataMsg *)conn->read_buffer;

    if (g_cancellable_is_cancelled(conn->cancellable)) {
        g_object_unref(conn);
        return;
    }

    if (error || bytes == 0) {
        /* Error or connection closed by peer */
        if (error)
            g_debug("Read error on connection %u: %s", conn->id, error->message);
        else
            g_debug("Connection %u reset by peer", conn->id);
        connection_close_unref(conn);
    } else {
        msg->id = conn->id;
        msg->size = bytes;
        FlexVDIMessageHeader * header = flexvdi_port_get_msg_buffer_header(conn->read_buffer);
        header->size = sizeof(*msg) + msg->size;
        flexvdi_port_send_msg(cf->port, FLEXVDI_FWDDATA, conn->read_buffer);
        conn->read_buffer = NULL;
        conn->data_sent += bytes;
        if (conn->data_sent < WINDOW_SIZE) {
            program_read(conn);
        } else {
            g_object_unref(conn);
        }
    }
}

static void connection_write_callback(GObject * source_object, GAsyncResult * res,
                                      gpointer user_data) {
    Connection * conn = (Connection *)user_data;
    GOutputStream * stream = (GOutputStream *)source_object;
    GBytes * bytes, * new_bytes;
    GError * error = NULL;
    int num_written = g_output_stream_write_finish(stream, res, &error);
    int remaining;

    if (error != NULL) {
        /* Error or connection closed by peer */
        g_debug("Write error on connection %u: %s", conn->id, error->message);
        connection_close_unref(conn);
    } else {
        bytes = (GBytes *)g_queue_pop_head(conn->write_buffer);
        g_debug("Written %d bytes on connection %u", num_written, conn->id);
        remaining = g_bytes_get_size(bytes) - num_written;
        if (remaining) {
            g_debug("Still %d bytes to go on connection %u", remaining, conn->id);
            new_bytes = g_bytes_new_from_bytes(bytes, num_written, remaining);
            g_queue_push_head(conn->write_buffer, new_bytes);
        }
        if(!g_queue_is_empty(conn->write_buffer)) {
            new_bytes = g_queue_peek_head(conn->write_buffer);
            g_output_stream_write_async(stream, g_bytes_get_data(new_bytes, NULL),
                                        g_bytes_get_size(new_bytes), G_PRIORITY_DEFAULT,
                                        NULL, connection_write_callback, conn);
        } else {
            g_object_unref(conn);
        }
        g_bytes_unref(bytes);

        conn->data_received += num_written;
        if (conn->data_received >= conn->ack_interval) {
            uint8_t * buf = flexvdi_port_get_msg_buffer(sizeof(FlexVDIForwardAckMsg));
            FlexVDIForwardAckMsg * msg = (FlexVDIForwardAckMsg *)buf;
            msg->id = conn->id;
            msg->size = conn->data_received;
            msg->winSize = conn->ack_interval * 2;
            conn->data_received = 0;
            flexvdi_port_send_msg(conn->cf->port, FLEXVDI_FWDACK, buf);
        }
    }
}

static void connection_connect_callback(GObject * source_object, GAsyncResult * res,
                                        gpointer user_data) {
    Connection * conn = (Connection *)user_data;

    if (g_cancellable_is_cancelled(conn->cancellable)) {
        g_object_unref(conn);
        return;
    }

    conn->conn = g_socket_client_connect_to_host_finish((GSocketClient *)source_object, res, NULL);
    if (!conn->conn) {
        /* Error */
        g_debug("Connection %u could not connect", conn->id);
        connection_close_unref(conn);
    } else {
        conn->connecting = FALSE;
        program_read(conn);
        uint8_t * buf = flexvdi_port_get_msg_buffer(sizeof(FlexVDIForwardAckMsg));
        FlexVDIForwardAckMsg * msg = (FlexVDIForwardAckMsg *)buf;
        msg->id = conn->id;
        msg->size = 0;
        msg->winSize = WINDOW_SIZE;
        flexvdi_port_send_msg(conn->cf->port, FLEXVDI_FWDACK, buf);
    }
}

static void handle_accepted(ConnForwarder * cf, FlexVDIForwardAcceptedMsg * msg) {
    gpointer id = GUINT_TO_POINTER(msg->id), rport = GUINT_TO_POINTER(msg->listenId);
    AddressPort * local;
    Connection * conn = g_hash_table_lookup(cf->connections, id);
    if (conn) {
        g_warning("Connection %u already exists.", msg->id);
        connection_close(conn);
    }

    local = ADDRESS_PORT(g_hash_table_lookup(cf->remote_assocs, rport));
    g_debug("Connection command, id %u on remote port %d -> %s port %d",
            msg->id, msg->listenId, local->address, local->port);
    if (local) {
        conn = connection_new_with_socket(cf, msg->id, msg->winSize / 2);
        if (conn) {
            g_hash_table_insert(cf->connections, id, g_object_ref(conn));
            g_socket_client_connect_to_host_async(conn->socket, local->address,
                                                  local->port, conn->cancellable,
                                                  connection_connect_callback, conn);
        } else {
            /* Error, close connection in agent */
            close_agent_connection(cf, msg->id);
        }
    } else {
        g_warning("Remote port %d is not associated with a local port.", msg->listenId);
        close_agent_connection(cf, msg->id);
    }

    g_free(msg);
}

static void handle_data(ConnForwarder * cf, FlexVDIForwardDataMsg * msg) {
    Connection * conn = g_hash_table_lookup(cf->connections, GUINT_TO_POINTER(msg->id));
    GBytes * chunk;
    GOutputStream * stream;

    if (!conn) {
        /* Ignore, this is usually an already closed connection */
        g_debug("Connection %u does not exist.", msg->id);
        g_free(msg);
    } else if (conn->connecting) {
        g_warning("Connection %u is still not connected!", conn->id);
        g_free(msg);
    } else {
        chunk = g_bytes_new_with_free_func(msg->data, msg->size, g_free, msg);
        g_queue_push_tail(conn->write_buffer, chunk);
        if (g_queue_get_length(conn->write_buffer) == 1) {
            stream = g_io_stream_get_output_stream((GIOStream *)conn->conn);
            g_output_stream_write_async(stream, g_bytes_get_data(chunk, NULL),
                                        g_bytes_get_size(chunk), G_PRIORITY_DEFAULT,
                                        NULL, connection_write_callback, g_object_ref(conn));
        }
    }
}

static void handle_close(ConnForwarder * cf, FlexVDIForwardCloseMsg * msg) {
    Connection * conn = g_hash_table_lookup(cf->connections, GUINT_TO_POINTER(msg->id));
    if (conn) {
        if (msg->error != 0)
            g_warning("Error in connection %u, closing: %s", conn->id, strerror(msg->error));
        else
            g_debug("Close command for connection %u", conn->id);
        connection_close(conn);
    } else {
        /* This is usually an already closed connection */
        g_debug("Connection %u does not exists.", msg->id);
        close_agent_connection(cf, msg->id);
    }

    g_free(msg);
}

static void handle_ack(ConnForwarder * cf, FlexVDIForwardAckMsg * msg) {
    Connection * conn = g_hash_table_lookup(cf->connections, GUINT_TO_POINTER(msg->id));
    g_debug("ACK command for connection %u with %d bytes", msg->id, (int)msg->size);
    if (conn) {
        if (conn->connecting) {
            conn->connecting = FALSE;
            conn->ack_interval = msg->winSize / 2;
            program_read(g_object_ref(conn));
        } else {
            guint32 data_sent_before = conn->data_sent;
            conn->data_sent -= msg->size;
            if (conn->data_sent < WINDOW_SIZE && data_sent_before >= WINDOW_SIZE) {
                program_read(g_object_ref(conn));
            }
        }
    } else {
        /* Ignore, this is usually an already closed connection */
        g_debug("Connection %u does not exists.", msg->id);
    }

    g_free(msg);
}

static gboolean conn_forwarder_handle_message(FlexvdiPort * port, int type, gpointer msg, gpointer data) {
    ConnForwarder * cf = CONN_FORWARDER(data);
    switch (type) {
        case FLEXVDI_FWDACCEPTED:
            handle_accepted(cf, (FlexVDIForwardAcceptedMsg *)msg);
            break;
        case FLEXVDI_FWDDATA:
            handle_data(cf, (FlexVDIForwardDataMsg *)msg);
            break;
        case FLEXVDI_FWDCLOSE:
            handle_close(cf, (FlexVDIForwardCloseMsg *)msg);
            break;
        case FLEXVDI_FWDACK:
            handle_ack(cf, (FlexVDIForwardAckMsg *)msg);
            break;
        default:
            return FALSE;
    }
    return TRUE;
}
