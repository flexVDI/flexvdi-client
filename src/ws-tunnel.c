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

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <gio/gio.h>

#include "ws-tunnel.h"

#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "flexvdi-ws"


#ifdef _WIN32
#define PF_LOCAL 0

static int socketpair(int domain, int type, int protocol, SOCKET fds[2]) {
    struct sockaddr_in inaddr;
    struct sockaddr addr;
    memset(&inaddr, 0, sizeof(inaddr));
    memset(&addr, 0, sizeof(addr));
    inaddr.sin_family = AF_INET;
    inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    inaddr.sin_port = 0;
    int len = sizeof(inaddr), result = 0;
    BOOL yesBool = 1;
    u_long yesUlong = 1;
    SOCKET lst;
    fds[0] = fds[1] = INVALID_SOCKET;

    if ((lst = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET ||
        setsockopt(lst, SOL_SOCKET, SO_REUSEADDR, (char*)&yesBool, sizeof(yesBool)) ||
        bind(lst, (struct sockaddr *)&inaddr, sizeof(inaddr)) ||
        listen(lst, 1) ||
        getsockname(lst, &addr, &len) ||
        (fds[0] = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET ||
        connect(fds[0], &addr, len) ||
        ioctlsocket(fds[0], FIONBIO, &yesUlong) == SOCKET_ERROR ||
        (fds[1] = accept(lst, 0, 0)) == INVALID_SOCKET) {
        closesocket(fds[0]);
        closesocket(fds[1]);
        result = -1;
    }
    closesocket(lst);
    return result;
}
#endif


struct _WsTunnel {
    GObject * parent;
    SoupMessage * msg;
    SpiceChannel * channel;
    gchar * channel_name;
    gint fd;
    GSocketConnection * local;
    SoupWebsocketConnection * ws_conn;
    GList * in_buffer;
    GCancellable * cancel;
};

enum {
    WS_TUNNEL_ERROR = 0,
    WS_TUNNEL_EOF,
    WS_TUNNEL_LAST_SIGNAL
};

static guint signals[WS_TUNNEL_LAST_SIGNAL];

G_DEFINE_TYPE(WsTunnel, ws_tunnel, G_TYPE_OBJECT);


static void ws_tunnel_dispose(GObject * obj);
static void ws_tunnel_finalize(GObject * obj);

static void ws_tunnel_class_init(WsTunnelClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->dispose = ws_tunnel_dispose;
    object_class->finalize = ws_tunnel_finalize;

    // Emited when there is a copy error
    signals[WS_TUNNEL_ERROR] =
        g_signal_new("error",
                     WS_TUNNEL_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_POINTER);

    // Emited when there is no more data
    signals[WS_TUNNEL_EOF] =
        g_signal_new("eof",
                     WS_TUNNEL_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);
}


static void ws_tunnel_init(WsTunnel * tunnel) {
#ifdef _WIN32
    SOCKET fd[2];
#else
    int fd[2];
#endif
    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd) == 0) {
        tunnel->fd = (gint)fd[0];
        tunnel->local = g_socket_connection_factory_create_connection(
            g_socket_new_from_fd(fd[1], NULL));
        tunnel->cancel = g_cancellable_new();
    }
}


static void ws_tunnel_dispose(GObject * obj) {
    WsTunnel * tunnel = WS_TUNNEL(obj);
    g_clear_object(&tunnel->msg);
    g_clear_object(&tunnel->channel);
    g_clear_object(&tunnel->local);
    g_clear_object(&tunnel->ws_conn);
    g_list_free_full(tunnel->in_buffer, (GDestroyNotify)g_bytes_unref);
    g_clear_object(&tunnel->cancel);
    G_OBJECT_CLASS(ws_tunnel_parent_class)->finalize(obj);
}


static void ws_tunnel_finalize(GObject * obj) {
    WsTunnel * tunnel = WS_TUNNEL(obj);
#ifdef _WIN32
    closesocket((SOCKET)tunnel->fd);
#else
    close(tunnel->fd);
#endif
    g_free(tunnel->channel_name);
    G_OBJECT_CLASS(ws_tunnel_parent_class)->finalize(obj);
}


static void ws_tunnel_connect(GObject *source_object, GAsyncResult * res,
                              gpointer user_data);

WsTunnel * ws_tunnel_new(SpiceChannel * channel, SoupSession * soup, gchar * ws_uri) {
    int id, type;

    g_object_get(channel, "channel-id", &id, "channel-type", &type, NULL);
    WsTunnel * tunnel = WS_TUNNEL(g_object_new(WS_TUNNEL_TYPE, NULL));
    tunnel->channel_name = g_strdup_printf("%d:%d", type, id);

    if (tunnel->fd != 0) {
        tunnel->channel = g_object_ref(channel);
        tunnel->msg = soup_message_new("GET", ws_uri);
        soup_session_websocket_connect_async(
            soup, tunnel->msg, NULL, NULL, NULL,
            ws_tunnel_connect, tunnel);
        g_debug("Created WS tunnel %s to uri %s",
            tunnel->channel_name, ws_uri);
        return g_object_ref(tunnel);
    } else {
        g_critical("Cannot create WS tunnel %s: socketpair failed",
            tunnel->channel_name);
        g_object_unref(tunnel);
        return NULL;
    }
}


gboolean ws_tunnel_is_channel(WsTunnel * tunnel, SpiceChannel * channel) {
    return tunnel->channel == channel;
}


static void on_ws_error(SoupWebsocketConnection * self, GError * error, gpointer user_data);
static void on_ws_msg(SoupWebsocketConnection * self, gint type,
                      GBytes * message, gpointer user_data);
static void on_ws_closed(SoupWebsocketConnection * self, gpointer user_data);
static void next_local_read(WsTunnel * tunnel);
static void read_local_finished(GObject * source_object, GAsyncResult * res,
                                gpointer user_data);
static void next_local_write(WsTunnel * tunnel);
static void write_local_finished(GObject * source_object, GAsyncResult * res,
                                 gpointer user_data);

void ws_tunnel_unref(WsTunnel * tunnel) {
    g_io_stream_close(G_IO_STREAM(tunnel->local), NULL, NULL);
    soup_websocket_connection_close(tunnel->ws_conn,
        SOUP_WEBSOCKET_CLOSE_NORMAL, "");
    g_cancellable_cancel(tunnel->cancel);
    g_object_unref(tunnel);
}


static void ws_tunnel_connect(GObject *source_object, GAsyncResult * res,
                              gpointer user_data) {
    WsTunnel * tunnel = WS_TUNNEL(user_data);
    SoupSession * soup = SOUP_SESSION(source_object);
    GError * error = NULL;

    tunnel->ws_conn = soup_session_websocket_connect_finish(soup, res, &error);
    if (error) {
        g_critical("IO error connecting WS tunnel %s: %s",
            tunnel->channel_name, error->message);
        g_signal_emit(tunnel, signals[WS_TUNNEL_ERROR], 0, error);
        g_object_unref(tunnel);
        return;
    }

    g_debug("WS tunnel %s connected", tunnel->channel_name);

    g_signal_connect(tunnel->ws_conn, "error", G_CALLBACK(on_ws_error), tunnel);
    g_signal_connect(tunnel->ws_conn, "message", G_CALLBACK(on_ws_msg), tunnel);
    g_signal_connect(tunnel->ws_conn, "closed", G_CALLBACK(on_ws_closed), tunnel);
    spice_channel_open_fd(tunnel->channel, tunnel->fd);
    next_local_read(tunnel);
}


static void next_local_read(WsTunnel * tunnel) {
    GInputStream * stream = g_io_stream_get_input_stream(G_IO_STREAM(tunnel->local));
    g_input_stream_read_bytes_async(
        stream, 4096, G_PRIORITY_DEFAULT, tunnel->cancel,
        read_local_finished, tunnel);
}


static void read_local_finished(GObject * source_object, GAsyncResult * res,
                                gpointer user_data) {
    WsTunnel * tunnel = WS_TUNNEL(user_data);
    GInputStream * stream = G_INPUT_STREAM(source_object);
    GError * error = NULL;
    GBytes * bytes = g_input_stream_read_bytes_finish(stream, res, &error);

    if (!g_cancellable_is_cancelled(tunnel->cancel)) {
        if (error) {
            g_signal_emit(tunnel, signals[WS_TUNNEL_ERROR], 0, error);
        } else {
            if (g_bytes_get_size(bytes) == 0) {
                g_debug("WS tunnel %s, local side closed",
                    tunnel->channel_name);
                g_signal_emit(tunnel, signals[WS_TUNNEL_EOF], 0);
            } else {
                g_debug("WS tunnel %s read %d bytes from local",
                    tunnel->channel_name, (int)g_bytes_get_size(bytes));
                soup_websocket_connection_send_binary(tunnel->ws_conn,
                    g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes));
                g_bytes_unref(bytes);
                next_local_read(tunnel);
            }
        }
    }
}


static void on_ws_error(SoupWebsocketConnection * self, GError * error, gpointer user_data) {
    WsTunnel * tunnel = WS_TUNNEL(user_data);
    g_critical("IO error in WS tunnel %s: %s", tunnel->channel_name, error->message);
    g_signal_emit(tunnel, signals[WS_TUNNEL_ERROR], 0, error);
}


static void on_ws_msg(SoupWebsocketConnection * self, gint type,
                      GBytes * message, gpointer user_data) {
    g_debug("on_ws_msg");
    WsTunnel * tunnel = WS_TUNNEL(user_data);
    if (!g_cancellable_is_cancelled(tunnel->cancel)) {
        g_debug("WS tunnel %s read %d bytes from ws", tunnel->channel_name,
            (int)g_bytes_get_size(message));
        tunnel->in_buffer = g_list_append(tunnel->in_buffer, g_bytes_ref(message));
        if (g_list_length(tunnel->in_buffer) == 1)
            next_local_write(tunnel);
    }
}


static void on_ws_closed(SoupWebsocketConnection * self, gpointer user_data) {
    WsTunnel * tunnel = WS_TUNNEL(user_data);
    g_debug("WS tunnel %s, ws side closed", tunnel->channel_name);
    g_signal_emit(tunnel, signals[WS_TUNNEL_EOF], 0);
}


static void next_local_write(WsTunnel * tunnel) {
    if (tunnel->in_buffer) {
        GBytes * bytes = (GBytes *)tunnel->in_buffer->data;
        GOutputStream * stream = g_io_stream_get_output_stream(G_IO_STREAM(tunnel->local));
        g_output_stream_write_bytes_async(
            stream, g_bytes_ref(bytes), G_PRIORITY_DEFAULT, tunnel->cancel,
            write_local_finished, tunnel);
    }
}


static void write_local_finished(GObject * source_object, GAsyncResult * res,
                                 gpointer user_data) {
    WsTunnel * tunnel = WS_TUNNEL(user_data);
    GOutputStream * stream = G_OUTPUT_STREAM(source_object);
    GError * error = NULL;
    gssize size = g_output_stream_write_bytes_finish(stream, res, &error);

    if (!g_cancellable_is_cancelled(tunnel->cancel)) {
        if (error) {
            g_signal_emit(tunnel, signals[WS_TUNNEL_ERROR], 0, error);
        } else {
            GBytes * bytes = (GBytes *)tunnel->in_buffer->data;
            gsize bsize = g_bytes_get_size(bytes);

            if (size < bsize) {
                tunnel->in_buffer->data = g_bytes_new_from_bytes(bytes, size, bsize - size);
            } else {
                tunnel->in_buffer = g_list_delete_link(tunnel->in_buffer, tunnel->in_buffer);
            }

            g_bytes_unref(bytes);
            next_local_write(tunnel);
        }
    }
}
