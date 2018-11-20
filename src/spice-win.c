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

#include "spice-win.h"
#include "flexvdi-port.h"
#include "printclient.h"
#include "about.h"

struct _SpiceWindow {
    GtkApplicationWindow parent;
    ClientConn * conn;
    ClientConf * conf;
    gint id;
    SpiceChannel * display_channel;
    gint width, height;
    gboolean initially_fullscreen;
    gboolean fullscreen;
    gboolean maximized;
    gint monitor;
    GtkRevealer * revealer;
    SpiceDisplay * spice;
    GtkBox * content_box;
    GtkToolbar * toolbar;
    GtkToolButton * close_button;
    GtkToolButton * copy_button;
    GtkToolButton * paste_button;
    GtkSeparatorToolItem * copy_paste_separator;
    GtkToolButton * fullscreen_button;
    GtkToolButton * restore_button;
    GtkToolButton * minimize_button;
    GtkSeparatorToolItem * power_actions_separator;
    GtkToolButton * reboot_button;
    GtkToolButton * shutdown_button;
    GtkToolButton * poweroff_button;
    GtkMenuButton * keys_button;
    GMenu * printers_menu;
    GtkMenuButton * printers_button;
    GSimpleActionGroup * printer_actions;
    GHashTable * printer_name_for_actions;
    GtkMenuButton * usb_button;
    GtkRevealer * notification_revealer;
    GtkLabel * notification;
    GtkToolButton * about_button;
    guint notification_timeout_id;
    gulong channel_event_handler_id;
};

enum {
    SPICE_WIN_USER_ACTIVITY = 0,
    SPICE_WIN_LAST_SIGNAL
};

static guint signals[SPICE_WIN_LAST_SIGNAL];

G_DEFINE_TYPE(SpiceWindow, spice_window, GTK_TYPE_APPLICATION_WINDOW);

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
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, copy_paste_separator);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, fullscreen_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, restore_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, minimize_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, power_actions_separator);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, reboot_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, shutdown_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, poweroff_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, keys_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, printers_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, usb_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, notification_revealer);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, notification);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SpiceWindow, about_button);

    // Emited when the user presses a key or moves the mouse
    signals[SPICE_WIN_USER_ACTIVITY] =
        g_signal_new("user-activity",
                     SPICE_WIN_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);
}

static void realize_window(GtkWidget * toplevel, gpointer user_data);
static gboolean configure_event(GtkWidget * widget, GdkEvent * event, gpointer user_data);
gboolean map_event(GtkWidget * widget, GdkEvent * event, gpointer user_data);
static void copy_from_guest(GtkToolButton * toolbutton, gpointer user_data);
static void paste_to_guest(GtkToolButton * toolbutton, gpointer user_data);
static gboolean window_state_cb(GtkWidget * widget, GdkEventWindowState * event,
                                gpointer user_data);
static void toggle_fullscreen(GtkToolButton * toolbutton, gpointer user_data);
static void minimize(GtkToolButton * toolbutton, gpointer user_data);
static void power_event_cb(GtkToolButton * toolbutton, gpointer user_data);
static void keystroke(GSimpleAction * action, GVariant * parameter, gpointer user_data);
static void show_about(GtkToolButton * toolbutton, gpointer user_data);

static void spice_window_get_printers(SpiceWindow * win);
static void share_current_printers(gpointer user_data);

static GActionEntry keystroke_entry[] = {
    { "keystroke", keystroke, "s", NULL, NULL },
};

static gboolean user_activity(SpiceWindow * win) {
    g_signal_emit(win, signals[SPICE_WIN_USER_ACTIVITY], 0);
    return GDK_EVENT_PROPAGATE;
}

static void spice_window_init(SpiceWindow * win) {
    gtk_widget_init_template(GTK_WIDGET(win));

    g_signal_connect_swapped(win->close_button, "clicked", G_CALLBACK(gtk_window_close), win);
    g_signal_connect(win->copy_button, "clicked", G_CALLBACK(copy_from_guest), win);
    g_signal_connect(win->paste_button, "clicked", G_CALLBACK(paste_to_guest), win);
    g_signal_connect(win->fullscreen_button, "clicked", G_CALLBACK(toggle_fullscreen), win);
    g_signal_connect(win->restore_button, "clicked", G_CALLBACK(toggle_fullscreen), win);
    g_signal_connect(win->minimize_button, "clicked", G_CALLBACK(minimize), win);
    g_signal_connect(win->reboot_button, "clicked", G_CALLBACK(power_event_cb), win);
    g_signal_connect(win->shutdown_button, "clicked", G_CALLBACK(power_event_cb), win);
    g_signal_connect(win->poweroff_button, "clicked", G_CALLBACK(power_event_cb), win);
    g_signal_connect(win, "window-state-event", G_CALLBACK(window_state_cb), win);
    g_signal_connect(win, "realize", G_CALLBACK(realize_window), win);
    g_signal_connect(win, "configure-event", G_CALLBACK(configure_event), NULL);
    g_signal_connect(win, "map-event", G_CALLBACK(map_event), NULL);
    g_signal_connect(win->about_button, "clicked", G_CALLBACK(show_about), win);

    g_action_map_add_action_entries(G_ACTION_MAP(win), keystroke_entry, 1, win);
    GtkBuilder * builder = gtk_builder_new_from_resource(
#ifdef WIN32
        "/com/flexvdi/client/keys-menu-windows.ui"
#else
        "/com/flexvdi/client/keys-menu-linux.ui"
#endif
    );
    GMenuModel * keys_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
    gtk_menu_button_set_menu_model(win->keys_button, keys_menu);
    g_object_unref(builder);

    win->printer_actions = g_simple_action_group_new();
    win->printers_menu = g_menu_new();
    win->printer_name_for_actions = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    gtk_widget_insert_action_group(GTK_WIDGET(win), "printer", G_ACTION_GROUP(win->printer_actions));
    gtk_menu_button_set_menu_model(win->printers_button, G_MENU_MODEL(win->printers_menu));
}

static void spice_window_dispose(GObject * obj) {
    SpiceWindow * win = SPICE_WIN(obj);
    g_clear_object(&win->conn);
    g_clear_object(&win->conf);
    if (win->display_channel)
        g_signal_handler_disconnect(win->display_channel, win->channel_event_handler_id);
    g_clear_object(&win->display_channel);
    if (win->printer_name_for_actions)
        g_hash_table_unref(win->printer_name_for_actions);
    win->printer_name_for_actions = NULL;
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
static void mouse_grab_cb(SpiceDisplay * display, gint status, gpointer user_data);

void usb_connect_failed(GObject * object, SpiceUsbDevice * device,
                        GError * error, gpointer user_data);

SpiceWindow * spice_window_new(ClientConn * conn, SpiceChannel * channel,
                               ClientConf * conf, int id, gchar * title) {
    SpiceWindow * win = g_object_new(SPICE_WIN_TYPE,
                                     "title", title,
                                     NULL);
    win->id = id;
    win->conn = g_object_ref(conn);
    win->conf = g_object_ref(conf);
    win->display_channel = g_object_ref(channel);
    win->channel_event_handler_id =
        g_signal_connect(channel, "channel-event", G_CALLBACK(channel_event), win);

    /* spice display */
    SpiceSession * session = client_conn_get_session(conn);
    win->spice = spice_display_new_with_monitor(session, 0, id);
    gtk_box_pack_end(win->content_box, GTK_WIDGET(win->spice), TRUE, TRUE, 0);
    gtk_widget_grab_focus(GTK_WIDGET(win->spice));

    g_signal_connect(G_OBJECT(win->spice), "motion-notify-event",
                     G_CALLBACK(motion_notify_event_cb), win);
    g_signal_connect(G_OBJECT(win->spice), "leave-notify-event",
                     G_CALLBACK(leave_window_cb), win);
    g_signal_connect(G_OBJECT(win->spice), "mouse-grab",
                     G_CALLBACK(mouse_grab_cb), win);
    g_signal_connect_swapped(win->spice, "scroll-event",
                             G_CALLBACK(user_activity), win);
    g_signal_connect_swapped(win->spice, "button-press-event",
                             G_CALLBACK(user_activity), win);
    g_signal_connect_swapped(win->spice, "button-release-event",
                             G_CALLBACK(user_activity), win);
    g_signal_connect_swapped(win->spice, "key-press-event",
                             G_CALLBACK(user_activity), win);
    g_signal_connect_swapped(win->spice, "key-release-event",
                             G_CALLBACK(user_activity), win);

    GtkWidget * usb_device_widget = spice_usb_device_widget_new(session, NULL); /* default format */
    g_signal_connect(usb_device_widget, "connect-failed",
                     G_CALLBACK(usb_connect_failed), win);
    GtkWidget * popover = gtk_popover_new(NULL);
    gtk_widget_set_name(GTK_WIDGET(popover), "usb-popover");
    gtk_container_add(GTK_CONTAINER(popover), usb_device_widget);
    gtk_menu_button_set_popover(win->usb_button, popover);
    gtk_widget_show_all(popover);

    win->initially_fullscreen = client_conf_get_fullscreen(conf);
    client_conf_set_display_options(conf, G_OBJECT(win->spice));
    SpiceGrabSequence *seq = spice_grab_sequence_new_from_string(client_conf_get_grab_sequence(conf));
    spice_display_set_grab_keys(win->spice, seq);
    spice_grab_sequence_free(seq);

    if (client_conf_get_disable_power_actions(conf)) {
        gtk_container_remove(GTK_CONTAINER(win->toolbar), GTK_WIDGET(win->power_actions_separator));
        gtk_container_remove(GTK_CONTAINER(win->toolbar), GTK_WIDGET(win->reboot_button));
        gtk_container_remove(GTK_CONTAINER(win->toolbar), GTK_WIDGET(win->poweroff_button));
        gtk_container_remove(GTK_CONTAINER(win->toolbar), GTK_WIDGET(win->shutdown_button));
    }
    if (client_conf_get_auto_clipboard(conf)) {
        gtk_container_remove(GTK_CONTAINER(win->toolbar), GTK_WIDGET(win->copy_paste_separator));
        gtk_container_remove(GTK_CONTAINER(win->toolbar), GTK_WIDGET(win->copy_button));
        gtk_container_remove(GTK_CONTAINER(win->toolbar), GTK_WIDGET(win->paste_button));
    }
    spice_window_get_printers(win);
    if (win->id == 0) {
        if (flexvdi_is_agent_connected())
            share_current_printers(win);
        else
            flexvdi_on_agent_connected(share_current_printers, win);
    }

    win->monitor = -1;
    if (client_conf_get_window_size(conf, id, &win->width, &win->height, &win->maximized, &win->monitor)) {
        gtk_window_set_default_size(GTK_WINDOW(win), win->width, win->height);
    }

    return win;
}

static void set_printers_menu_visibility(gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    if (!flexvdi_agent_supports_capability(FLEXVDI_CAP_PRINTING) ||
        g_menu_model_get_n_items(G_MENU_MODEL(win->printers_menu)) == 0) {
        gtk_widget_hide(GTK_WIDGET(win->printers_button));
    } else {
        gtk_widget_show(GTK_WIDGET(win->printers_button));
    }
}

static int get_monitor(SpiceWindow * win) {
    GdkDisplay * display = gdk_display_get_default();
    GdkWindow * gwin = gtk_widget_get_window(GTK_WIDGET(win));
    g_return_val_if_fail(gwin != NULL, 0);
    GdkMonitor * monitor = gdk_display_get_monitor_at_window(display, gwin);
    int i;
    for (i = 0; i < gdk_display_get_n_monitors(display); ++i)
        if (gdk_display_get_monitor(display, i) == monitor)
            return i;
    return 0;
}

static void realize_window(GtkWidget * toplevel, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);

    gtk_window_get_size(GTK_WINDOW(win), &win->width, &win->height);
    if (win->monitor == -1)
        win->monitor = get_monitor(win);
    client_conf_set_window_size(win->conf, win->id, win->width, win->height, win->maximized, win->monitor);

    if (flexvdi_is_agent_connected())
        set_printers_menu_visibility(win);
    else
        flexvdi_on_agent_connected(set_printers_menu_visibility, win);

#ifdef __APPLE__
    gtk_widget_hide(GTK_WIDGET(win->usb_button));
#endif

    GdkRectangle r;
    GdkDisplay * display = gdk_display_get_default();
    GdkMonitor * monitor = gdk_display_get_monitor(display, win->monitor);
    gdk_monitor_get_geometry(monitor, &r);
    gtk_window_move(GTK_WINDOW(win), r.x + (r.width - win->width) / 2, r.y + (r.height - win->height) / 2);
    if (win->maximized) gtk_window_maximize(GTK_WINDOW(win));

    // Do not go fullscreen until a map event is received, or it will fail in Windows.
}

static gboolean configure_event(GtkWidget * widget, GdkEvent * event, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(widget);

    win->monitor = get_monitor(win);
    // save the window geometry only if we are not maximized or fullscreen
    if (!(win->maximized || win->fullscreen)) {
        gtk_window_get_size(GTK_WINDOW(widget), &win->width, &win->height);
    }
    client_conf_set_window_size(win->conf, win->id, win->width, win->height, win->maximized, win->monitor);

    return GDK_EVENT_PROPAGATE;
}

gboolean map_event(GtkWidget * widget, GdkEvent * event, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(widget);

    if (win->initially_fullscreen) {
        gtk_window_fullscreen(GTK_WINDOW(win));
    } else {
        gtk_widget_show(GTK_WIDGET(win->fullscreen_button));
        gtk_widget_hide(GTK_WIDGET(win->restore_button));
        gtk_widget_hide(GTK_WIDGET(win->minimize_button));
        gtk_widget_hide(GTK_WIDGET(win->close_button));
    }

    return GDK_EVENT_PROPAGATE;
}

static void channel_event(SpiceChannel * channel, SpiceChannelEvent event, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);

    switch (event) {
    case SPICE_CHANNEL_CLOSED:
        client_conn_disconnect(win->conn, CLIENT_CONN_DISCONNECT_NO_ERROR);
        gtk_window_close(GTK_WINDOW(win));
        return;
    case SPICE_CHANNEL_ERROR_IO:
    case SPICE_CHANNEL_ERROR_TLS:
    case SPICE_CHANNEL_ERROR_LINK:
    case SPICE_CHANNEL_ERROR_CONNECT:
    case SPICE_CHANNEL_ERROR_AUTH:
        client_conn_disconnect(win->conn, CLIENT_CONN_DISCONNECT_CONN_ERROR);
        gtk_window_close(GTK_WINDOW(win));
    default:
        return;
    }
}

static void copy_from_guest(GtkToolButton * toolbutton, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    spice_gtk_session_paste_from_guest(
        spice_gtk_session_get(client_conn_get_session(win->conn))
    );
}

static void paste_to_guest(GtkToolButton * toolbutton, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    spice_gtk_session_copy_to_guest(
        spice_gtk_session_get(client_conn_get_session(win->conn))
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

static void minimize(GtkToolButton * toolbutton, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    gtk_window_iconify(GTK_WINDOW(win));
}

static gboolean window_state_cb(GtkWidget * widget, GdkEventWindowState * event,
                                gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);

    win->maximized =
        (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    win->fullscreen =
        (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
    win->monitor = get_monitor(win);
    client_conf_set_window_size(win->conf, win->id, win->width, win->height, win->maximized, win->monitor);
    client_conf_set_fullscreen(win->conf, win->fullscreen);

    if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
        if (win->fullscreen) {
            gtk_widget_hide(GTK_WIDGET(win->fullscreen_button));
            gtk_widget_show(GTK_WIDGET(win->restore_button));
            gtk_widget_show(GTK_WIDGET(win->minimize_button));
            gtk_widget_show(GTK_WIDGET(win->close_button));
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
            gtk_widget_hide(GTK_WIDGET(win->close_button));
            g_object_ref(win->toolbar);
            gtk_container_remove(GTK_CONTAINER(win->revealer),
                                 GTK_WIDGET(win->toolbar));
            gtk_box_pack_start(win->content_box, GTK_WIDGET(win->toolbar), FALSE, FALSE, 0);
            g_object_unref(win->toolbar);
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static void power_event_cb(GtkToolButton * toolbutton, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    const gchar * action_name = "";
    int event_type = 0;
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
    return G_SOURCE_REMOVE;
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
    user_activity(win);
    return GDK_EVENT_PROPAGATE;
}

static gboolean spice_win_hide_notification(gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    gtk_revealer_set_reveal_child(win->notification_revealer, FALSE);
    win->notification_timeout_id = g_timeout_add(
        gtk_revealer_get_transition_duration(win->notification_revealer),
        hide_widget_cb, win->notification_revealer);
    return G_SOURCE_REMOVE;
}

void spice_win_show_notification(SpiceWindow * win, const gchar * text, gint duration) {
    gtk_label_set_text(win->notification, text);
    gtk_widget_show(GTK_WIDGET(win->notification_revealer));
    gtk_revealer_set_reveal_child(win->notification_revealer, TRUE);
    // Cancel previous hide timer
    if (win->notification_timeout_id) {
        g_source_remove(win->notification_timeout_id);
        win->notification_timeout_id = 0;
    }
    win->notification_timeout_id = g_timeout_add(duration,
        spice_win_hide_notification, win);
}

static gboolean leave_window_cb(GtkWidget * widget, GdkEventCrossing * event,
                                gpointer user_data) {
    GdkEventMotion mevent = {
        .y = event->y
    };
    return motion_notify_event_cb(widget, &mevent, user_data);
}

static void mouse_grab_cb(SpiceDisplay * display, gint status, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);

    if (status) {
        gchar * grab_sequence = client_conf_get_grab_sequence(win->conf);
        g_autofree gchar * msg =
            g_strdup_printf("Press %s to release mouse", grab_sequence);
        spice_win_show_notification(win, msg, 5000);
    }
}

static void keystroke(GSimpleAction * action, GVariant * parameter, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    guint keyvals[3] = {
        GDK_KEY_Control_L,
        GDK_KEY_Alt_L,
        GDK_KEY_F1
    };
    int numKeys = 3;
    const gchar * combination = g_variant_get_string(parameter, NULL);

    if (!g_strcmp0(combination, "Ctrl+Alt+Supr")) {
        keyvals[2] = GDK_KEY_Delete;
    } else if (!g_strcmp0(combination, "Ctrl+Win+Right")) {
        keyvals[1] = GDK_KEY_Meta_L;
        keyvals[2] = GDK_KEY_Right;
    } else if (!g_strcmp0(combination, "Ctrl+Win+Left")) {
        keyvals[1] = GDK_KEY_Meta_L;
        keyvals[2] = GDK_KEY_Left;
    } else if (!g_strcmp0(combination, "Win+L")) {
        keyvals[0] = GDK_KEY_Meta_L;
        keyvals[1] = GDK_KEY_l;
        numKeys = 2;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F1")) {
        keyvals[2] = GDK_KEY_F1;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F2")) {
        keyvals[2] = GDK_KEY_F2;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F3")) {
        keyvals[2] = GDK_KEY_F3;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F4")) {
        keyvals[2] = GDK_KEY_F4;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F5")) {
        keyvals[2] = GDK_KEY_F5;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F6")) {
        keyvals[2] = GDK_KEY_F6;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F7")) {
        keyvals[2] = GDK_KEY_F7;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F8")) {
        keyvals[2] = GDK_KEY_F8;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F9")) {
        keyvals[2] = GDK_KEY_F9;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F10")) {
        keyvals[2] = GDK_KEY_F10;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F11")) {
        keyvals[2] = GDK_KEY_F11;
    } else if (!g_strcmp0(combination, "Ctrl+Alt+F12")) {
        keyvals[2] = GDK_KEY_F12;
    } else return;

    spice_display_send_keys(SPICE_DISPLAY(win->spice), keyvals, numKeys,
                            SPICE_DISPLAY_KEY_EVENT_PRESS | SPICE_DISPLAY_KEY_EVENT_RELEASE);
}

void spice_win_set_cp_sensitive(SpiceWindow * win, gboolean copy, gboolean paste) {
    gtk_widget_set_sensitive(GTK_WIDGET(win->copy_button), copy);
    gtk_widget_set_sensitive(GTK_WIDGET(win->paste_button), paste);
}

static void printer_toggled(GSimpleAction * action, GVariant * parameter, gpointer user_data);

static void spice_window_get_printers(SpiceWindow * win) {
    GSList * printers, * printer;
    g_debug("Getting printer list:");
    flexvdi_get_printer_list(&printers);
    for (printer = printers; printer != NULL; printer = g_slist_next(printer)) {
        const char * printer_name = (const char *)printer->data;
        g_autofree gchar * parsed_printer_name = g_strdup(printer_name);
        g_strcanon(parsed_printer_name,
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_",
            '_');
        gboolean state = client_conf_is_printer_shared(win->conf, printer_name);
        g_debug("  %s, %s", printer_name, state ? "shared" : "not shared");

        GSimpleAction * action = g_simple_action_new_stateful(parsed_printer_name, NULL,
            g_variant_new_boolean(state));
        g_action_map_add_action(G_ACTION_MAP(win->printer_actions), G_ACTION(action));
        g_signal_connect(G_OBJECT(action), "activate", G_CALLBACK(printer_toggled), win);
        g_hash_table_insert(win->printer_name_for_actions, action, g_strdup(printer_name));

        g_autofree gchar * full_action_name =
            g_strconcat("printer.", parsed_printer_name, NULL);
        GMenuItem * item = g_menu_item_new(printer_name, full_action_name);
        g_menu_append_item(win->printers_menu, item);
    }
    g_slist_free_full(printers, g_free);
    gtk_widget_hide(GTK_WIDGET(win->printers_button));
}

static gboolean set_printer_menu_item_sensitive(gpointer user_data) {
    GSimpleAction * action = G_SIMPLE_ACTION(user_data);
    g_simple_action_set_enabled(action, TRUE);
    return G_SOURCE_REMOVE;
}

typedef struct _ActionAndPrinter {
    GSimpleAction * action;
    gchar * printer;
} ActionAndPrinter;

static gpointer share_printer_thread(gpointer user_data) {
    ActionAndPrinter * data = (ActionAndPrinter *)user_data;
    flexvdi_share_printer(data->printer);
    g_idle_add(set_printer_menu_item_sensitive, data->action);
    g_free(data->printer);
    g_free(data);
    return NULL;
}

static void share_printer_async(GSimpleAction * action, const gchar * printer) {
    // flexvdi_share_printer can be a bit slow and "hang" the GUI
    g_simple_action_set_enabled(action, FALSE);
    ActionAndPrinter * data = g_new(ActionAndPrinter, 1);
    data->action = action;
    data->printer = g_strdup(printer);
    GThread * thread = g_thread_try_new(NULL, share_printer_thread,
                                        data, NULL);
    if (!thread) {
        share_printer_thread(data);
    } else {
        g_thread_unref(thread);
    }
}

static void printer_toggled(GSimpleAction * action, GVariant * parameter, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    const char * printer = g_hash_table_lookup(win->printer_name_for_actions, action);
    gboolean active = !g_variant_get_boolean(g_action_get_state(G_ACTION(action)));
    g_simple_action_set_state(action, g_variant_new_boolean(active));
    client_conf_share_printer(win->conf, printer, active);
    if (active) {
        share_printer_async(action, printer);
    } else {
        flexvdi_unshare_printer(printer);
    }
}

static void share_current_printers(gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    if (flexvdi_agent_supports_capability(FLEXVDI_CAP_PRINTING)) {
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init(&iter, win->printer_name_for_actions);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            GSimpleAction * action = G_SIMPLE_ACTION(key);
            if (g_variant_get_boolean(g_action_get_state(G_ACTION(action)))) {
                share_printer_async(action, value);
            }
        }
    }
}

static void show_about(GtkToolButton * toolbutton, gpointer user_data) {
    SpiceWindow * win = SPICE_WIN(user_data);
    client_show_about(GTK_WINDOW(win), win->conf);
}
