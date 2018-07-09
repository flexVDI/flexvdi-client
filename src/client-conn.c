/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include "client-conn.h"

struct _ClientConn {
    GObject parent;
    SpiceSession * session;
    SpiceGtkSession * gtk_session;
    SpiceMainChannel * main;
    SpiceAudio * audio;
    int channels;
    gboolean disconnecting;
    ClientConnDisconnectReason reason;
};

G_DEFINE_TYPE(ClientConn, client_conn, G_TYPE_OBJECT);

static void client_conn_dispose(GObject * obj);
static void client_conn_finalize(GObject * obj);

static void client_conn_class_init(ClientConnClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->dispose = client_conn_dispose;
    object_class->finalize = client_conn_finalize;
}

static void channel_new(SpiceSession * s, SpiceChannel * channel, gpointer data);
static void channel_destroy(SpiceSession * s, SpiceChannel * channel, gpointer data);

static void client_conn_init(ClientConn * conn) {
    conn->session = spice_session_new();
    conn->gtk_session = spice_gtk_session_get(conn->session);
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
        g_autofree gchar * ws_port =
            g_strdup_printf("%d", client_conf_get_port(conf));
        g_object_set(conn->session, "ws-port", ws_port, NULL);
    }

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

SpiceGtkSession * client_conn_get_gtk_session(ClientConn * conn) {
    return conn->gtk_session;
}

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

static void channel_destroy(SpiceSession * s, SpiceChannel * channel, gpointer data) {
    ClientConn * conn = CLIENT_CONN(data);
    int id, type;

    g_object_get(channel, "channel-id", &id, "channel-type", &type, NULL);
    g_debug("Destroyed Spice channel (%d:%d)", type, id);
    conn->channels--;

    if (conn->channels <= 0) {
        // What? Notify app?
        g_debug("No more channels left");
        g_object_unref(conn);
    }
}

SpiceMainChannel * client_conn_get_main_channel(ClientConn * conn) {
    return conn->main;
}
