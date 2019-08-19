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

#include "client-conn.h"
#include "ws-tunnel.h"
#ifdef ENABLE_SERIALREDIR
#include "serialredir.h"
#endif


struct _ClientConn {
    GObject parent;
    SpiceSession * session;
    SpiceMainChannel * main;
    SpiceAudio * audio;
    int channels;
    gboolean use_ws;
    gchar * ws_host, * ws_port, * ws_token;
    SoupSession * soup;
    GList * tunnels;
    gboolean disconnecting;
    ClientConnDisconnectReason reason;
    FlexvdiPort * guest_agent_port, * control_port;
};

enum {
    CLIENT_CONN_DISCONNECTED = 0,
    CLIENT_CONN_LAST_SIGNAL
};

static guint signals[CLIENT_CONN_LAST_SIGNAL];

G_DEFINE_TYPE(ClientConn, client_conn, G_TYPE_OBJECT);


static void client_conn_dispose(GObject * obj);
static void client_conn_finalize(GObject * obj);

static void client_conn_class_init(ClientConnClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->dispose = client_conn_dispose;
    object_class->finalize = client_conn_finalize;

    // Emited when the connection is disconnected
    signals[CLIENT_CONN_DISCONNECTED] =
        g_signal_new("disconnected",
                     CLIENT_CONN_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_INT);
}


static void channel_new(SpiceSession * s, SpiceChannel * channel, gpointer data);
static void channel_destroy(SpiceSession * s, SpiceChannel * channel, gpointer data);

static void client_conn_init(ClientConn * conn) {
    conn->guest_agent_port = flexvdi_port_new();
    conn->control_port = flexvdi_port_new();
    conn->session = spice_session_new();
    spice_set_session_option(conn->session);

    g_signal_connect(conn->session, "channel-new",
                     G_CALLBACK(channel_new), conn);
    g_signal_connect(conn->session, "channel-destroy",
                     G_CALLBACK(channel_destroy), conn);
    g_object_ref(conn);
}


static void client_conn_dispose(GObject * obj) {
    ClientConn * conn = CLIENT_CONN(obj);
    g_clear_object(&conn->session);
    g_clear_object(&conn->guest_agent_port);
    g_clear_object(&conn->control_port);
    g_list_free_full(conn->tunnels, (GDestroyNotify)ws_tunnel_unref);
    G_OBJECT_CLASS(client_conn_parent_class)->dispose(obj);
}


static void client_conn_finalize(GObject * obj) {
    ClientConn * conn = CLIENT_CONN(obj);
    g_free(conn->ws_host);
    g_free(conn->ws_port);
    g_free(conn->ws_token);
    G_OBJECT_CLASS(client_conn_parent_class)->finalize(obj);
}


ClientConn * client_conn_new(ClientConf * conf, JsonObject * params) {
    ClientConn * conn = CLIENT_CONN(g_object_new(CLIENT_CONN_TYPE, NULL));

    g_object_set(conn->session,
                 "password", json_object_get_string_member(params, "spice_password"),
                 NULL);
    conn->use_ws = json_object_has_member(params, "use_ws") && (
        json_object_get_boolean_member(params, "use_ws") ||
        !g_strcmp0(json_object_get_string_member(params, "use_ws"), "true"));
    if (conn->use_ws) {
        conn->ws_host = g_strdup(json_object_get_string_member(params, "spice_address"));
        const gchar * port = client_conf_get_port(conf);
        conn->ws_port = g_strdup(port ? port : "443");
        conn->ws_token = g_strdup(json_object_get_string_member(params, "spice_port"));
        conn->soup = client_conf_get_soup_session(conf);
    } else {
        g_object_set(conn->session,
                     "host", json_object_get_string_member(params, "spice_address"),
                     "port", json_object_get_string_member(params, "spice_port"),
                     NULL);
    }
    client_conf_set_session_options(conf, conn->session);

    return conn;
}


ClientConn * client_conn_new_with_uri(ClientConf * conf, const char * uri) {
    ClientConn * conn = CLIENT_CONN(g_object_new(CLIENT_CONN_TYPE, NULL));
    conn->use_ws = FALSE;
    g_object_set(conn->session, "uri", uri, NULL);
    client_conf_set_session_options(conf, conn->session);

    return conn;
}


void client_conn_connect(ClientConn * conn) {
    conn->disconnecting = FALSE;
    if (conn->use_ws)
        spice_session_open_fd(conn->session, -1);
    else
        spice_session_connect(conn->session);
}


void client_conn_disconnect(ClientConn * conn, ClientConnDisconnectReason reason) {
    if (conn->disconnecting)
        return;
    conn->disconnecting = TRUE;
    conn->reason = reason;
    if (conn->use_ws)
        soup_session_abort(conn->soup);
    spice_session_disconnect(conn->session);
}


SpiceSession * client_conn_get_session(ClientConn * conn) {
    return conn->session;
}


static void open_ws_tunnel(SpiceChannel * channel, int with_tls, gpointer user_data);
static void port_channel(SpiceChannel * channel, GParamSpec * pspec, ClientConn * conn);

/*
 * New channel handler. Finishes the connection process of each channel.
 * Besides, saves a reference to the main channel and sets up the audio channel.
 */
static void channel_new(SpiceSession * s, SpiceChannel * channel, gpointer data) {
    ClientConn * conn = CLIENT_CONN(data);
    int id, type;

    g_object_get(channel, "channel-id", &id, "channel-type", &type, NULL);
    g_debug("New %s channel (%d:%d)", spice_channel_type_to_string(type), type, id);
    conn->channels++;

    if (conn->use_ws)
        g_signal_connect(channel, "open-fd", G_CALLBACK(open_ws_tunnel), conn);

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        conn->main = SPICE_MAIN_CHANNEL(channel);
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        spice_channel_connect(channel);
    }

    if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        conn->audio = spice_audio_get(s, NULL);
    }

    if (SPICE_IS_WEBDAV_CHANNEL(channel)) {
        gchar * shared_dir = NULL;
        g_object_get(s, "shared-dir", &shared_dir, NULL);
        if (shared_dir != NULL)
            spice_channel_connect(channel);
    } else if (SPICE_IS_PORT_CHANNEL(channel)) {
        g_signal_connect(channel, "notify::port-name",
                         G_CALLBACK(port_channel), conn);
        spice_channel_connect(channel);
    }
}


/*
 * Channel destroy handler.
 */
static void channel_destroy(SpiceSession * s, SpiceChannel * channel, gpointer data) {
    ClientConn * conn = CLIENT_CONN(data);
    int id, type;

    g_object_get(channel, "channel-id", &id, "channel-type", &type, NULL);
    g_debug("Destroyed Spice channel (%d:%d)", type, id);
    conn->channels--;

    if (conn->channels <= 0) {
        g_debug("No more channels left");
        g_signal_emit(conn, signals[CLIENT_CONN_DISCONNECTED], 0, conn->reason);
        g_object_unref(conn);
    }
}


static void tunnel_error(WsTunnel * tunnel, GError * error, gpointer user_data);
static void tunnel_eof(WsTunnel * tunnel, gpointer user_data);

/*
 * open_ws_tunnel
 *
 * Create a WebSocket connection for this channel. Then, setup a socket pair. Pass one end to
 * spice_channel_open_fd and use the other one to forward communication to the WebSocket.
 */
static void open_ws_tunnel(SpiceChannel * channel, int with_tls, gpointer user_data) {
    ClientConn * conn = CLIENT_CONN(user_data);

    int id, type;
    g_object_get(channel, "channel-id", &id, "channel-type", &type, NULL);

    GList * tunnels;
    for (tunnels = conn->tunnels; tunnels; tunnels = tunnels->next)
        if (ws_tunnel_is_channel((WsTunnel *)tunnels->data, channel)) {
            g_debug("There is already a WS tunnel for channel %d:%d", type, id);
            return;
        }

    g_autofree gchar * uri = g_strdup_printf("wss://%s:%s/?ver=2&token=%s",
        conn->ws_host, conn->ws_port, conn->ws_token);
    g_debug("Creating a WS tunnel for channel %d:%d on uri %s", type, id, uri);
    WsTunnel * tunnel = ws_tunnel_new(channel, conn->soup, uri);
    if (!tunnel) {
        g_critical("Failed to create a WS tunnel");
        client_conn_disconnect(conn, CLIENT_CONN_DISCONNECT_IO_ERROR);
    } else {
        conn->tunnels = g_list_append(conn->tunnels, tunnel);
        g_signal_connect(tunnel, "error", G_CALLBACK(tunnel_error), conn);
        g_signal_connect(tunnel, "eof", G_CALLBACK(tunnel_eof), conn);
    }
}


static void tunnel_error(WsTunnel * tunnel, GError * error, gpointer user_data) {
    ClientConn * conn = CLIENT_CONN(user_data);
    g_error_free(error);
    client_conn_disconnect(conn, CLIENT_CONN_DISCONNECT_IO_ERROR);
}


static void tunnel_eof(WsTunnel * tunnel, gpointer user_data) {
    ClientConn * conn = CLIENT_CONN(user_data);
    client_conn_disconnect(conn, CLIENT_CONN_DISCONNECT_NO_ERROR);
}


static void port_channel(SpiceChannel * channel, GParamSpec * pspec, ClientConn * conn) {
    g_autofree gchar * name = NULL;

    g_object_get(channel, "port-name", &name, NULL);
    g_debug("Port channel %s", name);

    if (g_strcmp0(name, "es.flexvdi.guest_agent") == 0) {
        flexvdi_port_set_channel(conn->guest_agent_port, SPICE_PORT_CHANNEL(channel));
    } else if (g_strcmp0(name, "com.flexvdi.control") == 0) {
        flexvdi_port_set_channel(conn->control_port, SPICE_PORT_CHANNEL(channel));
#ifdef ENABLE_SERIALREDIR
    } else {
        serial_port_open(channel);
#endif
    }
}


SpiceMainChannel * client_conn_get_main_channel(ClientConn * conn) {
    return conn->main;
}


ClientConnDisconnectReason client_conn_get_reason(ClientConn * conn) {
    return conn->reason;
}


gchar * client_conn_get_reason_str(ClientConn * conn) {
    switch (conn->reason) {
        case CLIENT_CONN_DISCONNECT_NO_ERROR:
            return g_strdup("The connection was orderly closed by the server.");
        case CLIENT_CONN_DISCONNECT_USER:
            return g_strdup("The connection was closed by a user action.");
        case CLIENT_CONN_DISCONNECT_INACTIVITY:
            return g_strdup("The connection was closed due to inactivity.");
        case CLIENT_CONN_DISCONNECT_CONN_ERROR:
        case CLIENT_CONN_DISCONNECT_IO_ERROR:
        case CLIENT_CONN_DISCONNECT_AUTH_ERROR:
            return g_strdup("Connection error, see the log file for further information.");
        default:
            return g_strdup("The connection was closed for unknown reasons.");
    }
}


FlexvdiPort * client_conn_get_guest_agent_port(ClientConn * conn) {
    return conn->guest_agent_port;
}


FlexvdiPort * client_conn_get_control_port(ClientConn * conn) {
    return conn->control_port;
}

