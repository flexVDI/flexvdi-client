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
    GtkToolbar * toolbar;
    GtkToolButton * close_button;
    GtkToolButton * copy_button;
    GtkToolButton * paste_button;
    GtkToolButton * fullscreen_button;
    GtkToolButton * restore_button;
    GtkToolButton * minimize_button;
    GtkToolButton * reboot_button;
    GtkToolButton * shutdown_button;
    GtkToolButton * poweroff_button;
    GtkMenu * keys_menu;
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
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, toolbar);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, close_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, copy_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, paste_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, fullscreen_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, restore_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, minimize_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, reboot_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, shutdown_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, poweroff_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, keys_menu);
}

static void copy_from_guest(GtkToolButton * toolbutton, gpointer user_data);
static void paste_to_guest(GtkToolButton * toolbutton, gpointer user_data);
static gboolean window_state_cb(GtkWidget * widget, GdkEventWindowState * event,
                                gpointer user_data);
static void toggle_fullscreen(GtkToolButton * toolbutton, gpointer user_data);
static void power_event_cb(GtkToolButton * toolbutton, gpointer user_data);
static void keystroke_cb(GtkMenuItem * menuitem, gpointer user_data);

static void set_keystroke_cb(GtkWidget * widget, gpointer data) {
    g_signal_connect(widget, "activate", G_CALLBACK(keystroke_cb), data);
}

static void spice_window_init(SpiceWindow * win) {
    gtk_widget_init_template(GTK_WIDGET(win));

    g_signal_connect_swapped(win->close_button, "clicked", G_CALLBACK(gtk_window_close), win);
    g_signal_connect(win->copy_button, "clicked", G_CALLBACK(copy_from_guest), win);
    g_signal_connect(win->paste_button, "clicked", G_CALLBACK(paste_to_guest), win);
    g_signal_connect(win->fullscreen_button, "clicked", G_CALLBACK(toggle_fullscreen), win);
    g_signal_connect(win->restore_button, "clicked", G_CALLBACK(toggle_fullscreen), win);
    g_signal_connect(win->reboot_button, "clicked", G_CALLBACK(power_event_cb), win);
    g_signal_connect(win->shutdown_button, "clicked", G_CALLBACK(power_event_cb), win);
    g_signal_connect(win->poweroff_button, "clicked", G_CALLBACK(power_event_cb), win);
    g_signal_connect(win, "window-state-event", G_CALLBACK(window_state_cb), win);

    gtk_container_foreach(GTK_CONTAINER(win->keys_menu), set_keystroke_cb, win);
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

static void channel_event(SpiceChannel * channel, SpiceChannelEvent event, gpointer user_data);
static gboolean motion_notify_event_cb(GtkWidget * widget, GdkEventMotion * event,
                                       gpointer user_data);
static gboolean leave_window_cb(GtkWidget * widget, GdkEventCrossing * event,
                                gpointer user_data);

SpiceWindow * spice_window_new(ClientConn * conn, SpiceChannel * channel,
                               int id, gchar * title) {
    SpiceWindow * win = g_object_new(SPICE_WIN_TYPE,
                                     "title", title,
                                     NULL);
    win->id = id;
    win->conn = g_object_ref(conn);
    win->display_channel = g_object_ref(channel);
    g_signal_connect(channel, "channel-event", G_CALLBACK(channel_event), win);

    /* spice display */
    SpiceSession * session = client_conn_get_session(conn);
    win->spice = spice_display_new_with_monitor(session, 0, id);
    SpiceGrabSequence * seq = spice_grab_sequence_new_from_string("Shift_L+F12");
    spice_display_set_grab_keys(win->spice, seq);
    spice_grab_sequence_free(seq);
    gtk_box_pack_end(win->content_box, GTK_WIDGET(win->spice), TRUE, TRUE, 0);
    gtk_widget_grab_focus(GTK_WIDGET(win->spice));

    g_signal_connect(G_OBJECT(win->spice), "motion-notify-event",
                     G_CALLBACK(motion_notify_event_cb), win);
    g_signal_connect(G_OBJECT(win->spice), "leave-notify-event",
                     G_CALLBACK(leave_window_cb), win);

    return win;
}

static void channel_event(SpiceChannel * channel, SpiceChannelEvent event, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);

    switch (event) {
    case SPICE_CHANNEL_CLOSED:
    case SPICE_CHANNEL_ERROR_IO:
    case SPICE_CHANNEL_ERROR_TLS:
    case SPICE_CHANNEL_ERROR_LINK:
    case SPICE_CHANNEL_ERROR_CONNECT:
    case SPICE_CHANNEL_ERROR_AUTH:
        gtk_window_close(GTK_WINDOW(win));
    default:
        return;
    }
}

static void copy_from_guest(GtkToolButton * toolbutton, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    spice_gtk_session_paste_from_guest(
        client_conn_get_gtk_session(win->conn)
    );
}

static void paste_to_guest(GtkToolButton * toolbutton, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    spice_gtk_session_copy_to_guest(
        client_conn_get_gtk_session(win->conn)
    );
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
            g_object_ref(win->toolbar);
            gtk_container_remove(GTK_CONTAINER(win->content_box),
                                 GTK_WIDGET(win->toolbar));
            gtk_container_add(GTK_CONTAINER(win->revealer), GTK_WIDGET(win->toolbar));
            gtk_revealer_set_reveal_child(win->revealer, TRUE);
            g_object_unref(win->toolbar);
            gtk_widget_grab_focus(GTK_WIDGET(win->spice));
        } else {
            gtk_widget_show(GTK_WIDGET(win->fullscreen_button));
            gtk_widget_hide(GTK_WIDGET(win->restore_button));
            gtk_widget_hide(GTK_WIDGET(win->minimize_button));
            g_object_ref(win->toolbar);
            gtk_container_remove(GTK_CONTAINER(win->revealer),
                                 GTK_WIDGET(win->toolbar));
            gtk_box_pack_start(win->content_box, GTK_WIDGET(win->toolbar), FALSE, FALSE, 0);
            g_object_unref(win->toolbar);
        }
    }
    return TRUE;
}

static void power_event_cb(GtkToolButton * toolbutton, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    const gchar * action_name;
    int event_type;
    if (toolbutton == win->reboot_button) {
        action_name = "reset";
        event_type = SPICE_POWER_EVENT_RESET;
    } else if (toolbutton == win->shutdown_button) {
        action_name = "orderly shutdown";
        event_type = SPICE_POWER_EVENT_POWERDOWN;
    } else if (toolbutton == win->poweroff_button) {
        action_name = "immediately power off";
        event_type = SPICE_POWER_EVENT_SHUTDOWN;
    }

    GtkWidget * dialog = gtk_message_dialog_new(GTK_WINDOW(win),
                                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
                                                "WARNING: Are you sure you want to %s your desktop?",
                                                action_name);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (result == GTK_RESPONSE_YES) {
        spice_main_power_event_request(client_conn_get_main_channel(win->conn), event_type);
    }
}

static gboolean hide_widget_cb(gpointer user_data) {
    GtkRevealer * revealer = GTK_REVEALER(user_data);
    if (!gtk_revealer_get_reveal_child(revealer))
        gtk_widget_hide(GTK_WIDGET(revealer));
    return FALSE;
}

static gboolean motion_notify_event_cb(GtkWidget * widget, GdkEventMotion * event,
                                       gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    if (win->fullscreen) {
        if (event->y == 0.0) {
            gtk_widget_show(GTK_WIDGET(win->revealer));
            gtk_revealer_set_reveal_child(win->revealer, TRUE);
        } else if (gtk_revealer_get_reveal_child(win->revealer)) {
            gtk_revealer_set_reveal_child(win->revealer, FALSE);
            g_timeout_add(gtk_revealer_get_transition_duration(win->revealer),
                          hide_widget_cb, win->revealer);
        }
    }
    return FALSE;
}

static gboolean leave_window_cb(GtkWidget * widget, GdkEventCrossing * event,
                                gpointer user_data) {
    GdkEventMotion mevent = {
        .y = event->y
    };
    return motion_notify_event_cb(widget, &mevent, user_data);
}

static void keystroke_cb(GtkMenuItem * menuitem, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    guint keyvals[3] = {
        GDK_KEY_Control_L,
        GDK_KEY_Alt_L,
        GDK_KEY_F1
    };
    int numKeys = 3;

    if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+Supr")) {
        keyvals[2] = GDK_KEY_Delete;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Win+Right")) {
        keyvals[1] = GDK_KEY_Meta_L;
        keyvals[2] = GDK_KEY_Right;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Win+Left")) {
        keyvals[1] = GDK_KEY_Meta_L;
        keyvals[2] = GDK_KEY_Left;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Win+L")) {
        keyvals[0] = GDK_KEY_Meta_L;
        keyvals[1] = GDK_KEY_l;
        numKeys = 2;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F1")) {
        keyvals[2] = GDK_KEY_F1;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F2")) {
        keyvals[2] = GDK_KEY_F2;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F3")) {
        keyvals[2] = GDK_KEY_F3;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F4")) {
        keyvals[2] = GDK_KEY_F4;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F5")) {
        keyvals[2] = GDK_KEY_F5;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F6")) {
        keyvals[2] = GDK_KEY_F6;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F7")) {
        keyvals[2] = GDK_KEY_F7;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F8")) {
        keyvals[2] = GDK_KEY_F8;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F9")) {
        keyvals[2] = GDK_KEY_F9;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F10")) {
        keyvals[2] = GDK_KEY_F10;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F11")) {
        keyvals[2] = GDK_KEY_F11;
    } else if (!g_strcmp0(gtk_menu_item_get_label(menuitem), "Ctrl+Alt+F12")) {
        keyvals[2] = GDK_KEY_F12;
    } else return;

    spice_display_send_keys(SPICE_DISPLAY(win->spice), keyvals, numKeys,
                            SPICE_DISPLAY_KEY_EVENT_PRESS | SPICE_DISPLAY_KEY_EVENT_RELEASE);
}
