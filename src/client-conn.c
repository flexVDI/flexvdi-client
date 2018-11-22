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


struct _ClientConn {
    GObject parent;
    SpiceSession * session;
    SpiceMainChannel * main;
    SpiceAudio * audio;
    int channels;
    gboolean disconnecting;
    ClientConnDisconnectReason reason;
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
    G_OBJECT_CLASS(client_conn_parent_class)->dispose(obj);
}


static void client_conn_finalize(GObject * obj) {
    G_OBJECT_CLASS(client_conn_parent_class)->finalize(obj);
}


ClientConn * client_conn_new(ClientConf * conf, JsonObject * params) {
    ClientConn * conn = CLIENT_CONN(g_object_new(CLIENT_CONN_TYPE, NULL));

    g_object_set(conn->session,
                 "host", json_object_get_string_member(params, "spice_address"),
                 "port", json_object_get_string_member(params, "spice_port"),
                 "password", json_object_get_string_member(params, "spice_password"),
                 NULL);
    if (json_object_get_boolean_member(params, "use_ws")) {
        const gchar * port = client_conf_get_port(conf);
        g_object_set(conn->session, "ws-port", port ? port : "443", NULL);
    }

    client_conf_set_session_options(conf, conn->session);

    return conn;
}


ClientConn * client_conn_new_with_uri(ClientConf * conf, const char * uri) {
    ClientConn * conn = CLIENT_CONN(g_object_new(CLIENT_CONN_TYPE, NULL));
    g_object_set(conn->session, "uri", uri, NULL);
    client_conf_set_session_options(conf, conn->session);

    return conn;
}


void client_conn_connect(ClientConn * conn) {
    conn->disconnecting = FALSE;
    spice_session_connect(conn->session);
}


void client_conn_disconnect(ClientConn * conn, ClientConnDisconnectReason reason) {
    if (conn->disconnecting)
        return;
    conn->disconnecting = TRUE;
    conn->reason = reason;
    spice_session_disconnect(conn->session);
}


SpiceSession * client_conn_get_session(ClientConn * conn) {
    return conn->session;
}


/*
 * New channel handler. Finishes the connection process of each channel.
 * Besides, saves a reference to the main channel and sets up the audio channel.
 */
static void channel_new(SpiceSession * s, SpiceChannel * channel, gpointer data) {
    ClientConn * conn = CLIENT_CONN(data);
    int id, type;

    g_object_get(channel, "channel-id", &id, "channel-type", &type, NULL);
    g_debug("New Spice channel (%d:%d)", type, id);
    conn->channels++;

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        conn->main = SPICE_MAIN_CHANNEL(channel);
    }

    if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        conn->audio = spice_audio_get(s, NULL);
    }

    spice_channel_connect(channel);
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
