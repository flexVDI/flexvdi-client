/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include <spice-client-gtk.h>

#include "client-conn.h"

struct _ClientConn {
    GObject parent;
    ClientConf * conf;
    SpiceSession * session;
    SpiceGtkSession * gtk_session;
    SpiceMainChannel * main;
    GList * wins;
    SpiceAudio * audio;
    const char * mouse_state;
    const char * agent_state;
    gboolean agent_connected;
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

static void client_conn_init(ClientConn * conn) {
}

static void client_conn_dispose(GObject * obj) {
    ClientConn * conn = CLIENT_CONN(obj);
    g_clear_object(&conn->conf);
    G_OBJECT_CLASS(client_conn_parent_class)->dispose(obj);
}

static void client_conn_finalize(GObject * obj) {
    G_OBJECT_CLASS(client_conn_parent_class)->finalize(obj);
}

ClientConn * client_conn_new(ClientConf * conf, JsonObject * params) {
    ClientConn * conn = g_object_new(CLIENT_CONN_TYPE, NULL);
    conn->conf = g_object_ref(conf);
    conn->session = spice_session_new();
    conn->gtk_session = spice_gtk_session_get(conn->session);
    spice_set_session_option(conn->session);
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

    /*
    g_object_set(conn->gtk_session,
                 "disable-copy-to-guest", disable_copy_to_guest,
                 "disable-paste-from-guest", disable_paste_from_guest,
                 NULL);
    g_signal_connect(conn->session, "channel-new",
                     G_CALLBACK(channel_new), conn);
    g_signal_connect(conn->session, "channel-destroy",
                     G_CALLBACK(channel_destroy), conn);
    g_signal_connect(conn->session, "notify::migration-state",
                     G_CALLBACK(migration_state), conn);

    manager = spice_usb_device_manager_get(conn->session, NULL);
    if (manager) {
        g_signal_connect(manager, "auto-connect-failed",
                         G_CALLBACK(usb_connect_failed), NULL);
        g_signal_connect(manager, "device-error",
                         G_CALLBACK(usb_connect_failed), NULL);
    }

    conn->transfers = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                            g_object_unref,
                                            (GDestroyNotify)transfer_task_widgets_free);
    */
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
