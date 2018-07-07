/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include "spice-win.h"

struct _SpiceWindow {
    GtkWindow parent;
    ClientConn * conn;
    gint id;
    SpiceChannel * display_channel;
    gboolean fullscreen;
    GtkRevealer * revealer;
    SpiceDisplay * spice;
    GtkBox * content_box;
    GtkBox * toolbar_box;
    GtkToolButton * fullscreen_button;
    GtkToolButton * restore_button;
    GtkToolButton * minimize_button;
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
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, revealer);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, content_box);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, toolbar_box);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, fullscreen_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, restore_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, minimize_button);
}

static gboolean window_state_cb(GtkWidget * widget, GdkEventWindowState * event,
                                gpointer user_data);
static void toggle_fullscreen(GtkToolButton * toolbutton, gpointer user_data);

static void spice_window_init(SpiceWindow * win) {
    gtk_widget_init_template(GTK_WIDGET(win));

    g_signal_connect(win->fullscreen_button, "clicked", G_CALLBACK(toggle_fullscreen), win);
    g_signal_connect(win->restore_button, "clicked", G_CALLBACK(toggle_fullscreen), win);
    g_signal_connect(win, "window-state-event", G_CALLBACK(window_state_cb), win);
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
    gtk_box_pack_end(win->content_box, GTK_WIDGET(win->spice), TRUE, TRUE, 0);
    gtk_widget_grab_focus(GTK_WIDGET(win->spice));

    return win;
}

static void toggle_fullscreen(GtkToolButton * toolbutton, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    if (win->fullscreen) {
        gtk_window_unfullscreen(GTK_WINDOW(win));
    } else {
        gtk_window_fullscreen(GTK_WINDOW(win));
    }
}

static gboolean window_state_cb(GtkWidget * widget, GdkEventWindowState * event,
                                gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
        win->fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;
        if (win->fullscreen) {
            gtk_widget_hide(GTK_WIDGET(win->fullscreen_button));
            gtk_widget_show(GTK_WIDGET(win->restore_button));
            gtk_widget_show(GTK_WIDGET(win->minimize_button));
            g_object_ref(win->toolbar_box);
            gtk_container_remove(GTK_CONTAINER(win->content_box),
                                 GTK_WIDGET(win->toolbar_box));
            gtk_container_add(GTK_CONTAINER(win->revealer), GTK_WIDGET(win->toolbar_box));
            gtk_revealer_set_reveal_child(win->revealer, TRUE);
            g_object_unref(win->toolbar_box);
            gtk_widget_grab_focus(GTK_WIDGET(win->spice));
        } else {
            gtk_widget_show(GTK_WIDGET(win->fullscreen_button));
            gtk_widget_hide(GTK_WIDGET(win->restore_button));
            gtk_widget_hide(GTK_WIDGET(win->minimize_button));
            g_object_ref(win->toolbar_box);
            gtk_container_remove(GTK_CONTAINER(win->revealer),
                                 GTK_WIDGET(win->toolbar_box));
            gtk_box_pack_start(win->content_box, GTK_WIDGET(win->toolbar_box), FALSE, FALSE, 0);
            g_object_unref(win->toolbar_box);
        }
    }
    return TRUE;
}
