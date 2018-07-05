/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include "spice-win.h"

struct _SpiceWindow {
    GtkWindow parent;
    ClientConn * conn;
    gint id;
    SpiceChannel * display_channel;
    SpiceDisplay * spice;
    GtkBox * content_box;
};

G_DEFINE_TYPE(SpiceWindow, spice_window, GTK_TYPE_WINDOW);

static void spice_window_dispose(GObject * obj);
static void spice_window_finalize(GObject * obj);

static void spice_window_class_init(SpiceWindowClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->dispose = spice_window_dispose;
    object_class->finalize = spice_window_finalize;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                                "/com/flexvdi/client/spice-win.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, content_box);
}

static void spice_window_init(SpiceWindow * win) {
    gtk_widget_init_template(GTK_WIDGET(win));
    // g_signal_connect(G_OBJECT(win), "window-state-event",
    //                  G_CALLBACK(window_state_cb), win);
}

static void spice_window_dispose(GObject * obj) {
    SpiceWindow * win = SPICE_WIN(obj);
    g_clear_object(&win->conn);
    g_clear_object(&win->display_channel);
    G_OBJECT_CLASS(spice_window_parent_class)->dispose(obj);
}

static void spice_window_finalize(GObject * obj) {
    SpiceWindow * win = SPICE_WIN(obj);
    g_debug("destroy window (#%d)", win->id);
    G_OBJECT_CLASS(spice_window_parent_class)->finalize(obj);
}

SpiceWindow * spice_window_new(ClientConn * conn, SpiceChannel * channel,
                               int id, gchar * title) {
    SpiceWindow * win = g_object_new(SPICE_WIN_TYPE,
                                     "title", title,
                                     NULL);
    win->id = id;
    win->conn = g_object_ref(conn);
    win->display_channel = g_object_ref(channel);

    /* spice display */
    SpiceSession * session = client_conn_get_session(conn);
    win->spice = spice_display_new_with_monitor(session, 0, id);
    SpiceGrabSequence * seq = spice_grab_sequence_new_from_string("Shift_L+F12");
    spice_display_set_grab_keys(win->spice, seq);
    spice_grab_sequence_free(seq);
    gtk_box_pack_start(win->content_box, GTK_WIDGET(win->spice), TRUE, TRUE, 0);

    return win;
}
