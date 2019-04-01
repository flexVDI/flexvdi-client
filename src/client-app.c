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

#include <stdio.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <spice-client-gtk.h>

#include "client-app.h"
#include "client-log.h"
#include "configuration.h"
#include "client-win.h"
#include "client-request.h"
#include "client-conn.h"
#include "spice-win.h"
#include "flexvdi-port.h"
#include "serialredir.h"
#include "printclient.h"
#include "about.h"

#define MAX_WINDOWS 16


#ifdef _WIN32
static void print_to_dialog(const gchar * string) {
    gtk_icon_theme_add_resource_path(gtk_icon_theme_get_default(), "/com/flexvdi/client/icons");
    GtkWidget * dialog = GTK_WIDGET(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    g_object_set(dialog, "title", "Help",
        "default-width", 800, "default-height", 600, "window-position", GTK_WIN_POS_CENTER,
        "icon", gdk_pixbuf_new_from_resource("/com/flexvdi/client/images/icon.png", NULL),
        "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG, NULL);
    GtkWidget * scroll = gtk_scrolled_window_new(NULL, NULL);
    g_object_set(scroll, "hexpand", TRUE, "vexpand", TRUE, NULL);
    GtkWidget * view = gtk_text_view_new();
    g_object_set(view, "editable", FALSE, "monospace", TRUE, "wrap-mode", GTK_WRAP_WORD, NULL);
    GtkTextBuffer * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_set_text(buffer, string, -1);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    gtk_container_add(GTK_CONTAINER(dialog), scroll);
    gtk_widget_show_all(dialog);

    GMainLoop * loop = g_main_loop_new(NULL, FALSE);
    g_signal_connect_swapped(dialog, "unmap", G_CALLBACK(g_main_loop_quit), loop);
    g_signal_connect_swapped(dialog, "delete-event", G_CALLBACK(g_main_loop_quit), loop);
    g_signal_connect_swapped(dialog, "destroy", G_CALLBACK(g_main_loop_quit), loop);
    g_main_loop_run(loop);

    gtk_widget_destroy(dialog);
    g_main_loop_unref(loop);
}
#endif
static GPrintFunc old_print_func = NULL;


struct _ClientApp {
    GtkApplication parent;
    ClientConf * conf;
    ClientAppWindow * main_window;
    ClientRequest * current_request;
    ClientConn * connection;
    const gchar * username;
    const gchar * password;
    const gchar * desktop;
    gchar * desktop_name;
    GHashTable * desktops;
    SpiceMainChannel * main;
    SpiceWindow * windows[MAX_WINDOWS];
    gint64 last_input_time;
    gboolean autologin;
};

G_DEFINE_TYPE(ClientApp, client_app, GTK_TYPE_APPLICATION);


static gboolean client_app_local_command_line(GApplication * gapp, gchar *** arguments,
                                              int * exit_status);
static void client_app_startup(GApplication * gapp);
static void client_app_activate(GApplication * gapp);
static void client_app_open(GApplication * application, GFile ** files,
                            gint n_files, const gchar * hint);

static void client_app_class_init(ClientAppClass * class) {
    G_APPLICATION_CLASS(class)->local_command_line = client_app_local_command_line;
    G_APPLICATION_CLASS(class)->startup = client_app_startup;
    G_APPLICATION_CLASS(class)->activate = client_app_activate;
    G_APPLICATION_CLASS(class)->open = client_app_open;
}


/*
 * Initialize the application instance. Called just after client_app_new by GObject.
 */
static void client_app_init(ClientApp * app) {
#ifdef _WIN32
    old_print_func = g_set_print_handler(print_to_dialog);
    // Call gtk_init here because normally it is called from GtkApplication::startup,
    // which is too late, after the --help* options have already been processed.
    gtk_init(NULL, NULL);
#else
    old_print_func = g_set_print_handler(print_to_stdout);
#endif

    // Create the configuration object. Reads options from config file.
    app->conf = client_conf_new();
    app->username = app->password = app->desktop = "";
    app->desktops = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    // Sets valid command-line options
    client_conf_set_application_options(app->conf, G_APPLICATION(app));
    g_application_add_option_group(G_APPLICATION(app), gst_init_get_option_group());
    g_application_set_option_context_parameter_string(G_APPLICATION(app), "[Spice URI]");
    g_application_set_option_context_summary(G_APPLICATION(app),
        "flexVDI Client is a Virtual Desktop client for flexVDI platforms. "
        "It can also be used as a generic Spice client providing a Spice URI on the command line.");
}


ClientApp * client_app_new(void) {
    return g_object_new(CLIENT_APP_TYPE,
                        "application-id", "com.flexvdi.client",
                        "flags", G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN,
                        NULL);
}


static gboolean client_app_local_command_line(GApplication * gapp, gchar *** arguments,
                                              int * exit_status) {
    ClientApp * app = CLIENT_APP(gapp);
    client_conf_set_original_arguments(app->conf, *arguments);
    return G_APPLICATION_CLASS(client_app_parent_class)->local_command_line(gapp, arguments, exit_status);
}


static void button_pressed_handler(ClientAppWindow * win, int button, gpointer user_data);
static gboolean key_event_handler(GtkWidget * widget, GdkEvent * event, gpointer user_data);
static void desktop_selected_handler(ClientAppWindow * win, gpointer user_data);
static gboolean delete_cb(GtkWidget * widget, GdkEvent * event, gpointer user_data);
static void network_changed(GNetworkMonitor * net_monitor, gboolean network_available, gpointer user_data);

static void client_app_configure(ClientApp * app, const gchar * error);
static void client_app_show_login(ClientApp * app, const gchar * error);
static void client_app_connect_with_spice_uri(ClientApp * app, const gchar * uri);

static void about_activated(GSimpleAction * action, GVariant * parameter, gpointer gapp) {
    ClientApp * app = CLIENT_APP(gapp);
    GtkWindow * active_window = gtk_application_get_active_window(GTK_APPLICATION(app));
    if (active_window)
        client_show_about(active_window, app->conf);
}

static void preferences_activated(GSimpleAction * action, GVariant * parameter, gpointer gapp) {
    client_app_configure(CLIENT_APP(gapp), NULL);
}

static void quit_activated(GSimpleAction * action, GVariant * parameter, gpointer gapp) {
    g_application_quit(G_APPLICATION(gapp));
}

static GActionEntry app_entries[] = {
    { "about", about_activated, NULL, NULL, NULL },
    { "preferences", preferences_activated, NULL, NULL, NULL },
    { "quit", quit_activated, NULL, NULL, NULL }
};

static void client_app_startup(GApplication * gapp) {
    g_set_print_handler(old_print_func);

    flexvdi_init_print_client();
#ifdef ENABLE_SERIALREDIR
    ClientApp * app = CLIENT_APP(gapp);
    serial_port_init(app->conf);
#endif

    G_APPLICATION_CLASS(client_app_parent_class)->startup(gapp);

    g_action_map_add_action_entries(G_ACTION_MAP(gapp), app_entries,
        G_N_ELEMENTS(app_entries), gapp);
}

/*
 * Activate application, called when no URI is provided in the command-line.
 * Sets up the application window, and connects automatically if an URI was provided.
 */
static void client_app_activate(GApplication * gapp) {
    ClientApp * app = CLIENT_APP(gapp);
    app->main_window = client_app_window_new(app);
    gtk_widget_show_all(GTK_WIDGET(app->main_window));

    const gchar * tid = client_conf_get_terminal_id(app->conf);
    g_autofree gchar * text = g_strconcat("Terminal ID: ", tid, NULL);
    client_app_window_set_info(app->main_window, text);

    g_signal_connect(app->main_window, "button-pressed",
        G_CALLBACK(button_pressed_handler), app);
    g_signal_connect(app->main_window, "key-press-event",
        G_CALLBACK(key_event_handler), app);
    g_signal_connect(app->main_window, "desktop-selected",
        G_CALLBACK(desktop_selected_handler), app);
    g_signal_connect(app->main_window, "delete-event",
        G_CALLBACK(delete_cb), app);

    GNetworkMonitor * net_monitor = g_network_monitor_get_default();
    g_debug("Using network monitor @0x%p, connectivity %d", net_monitor, g_network_monitor_get_connectivity(net_monitor));
    if (g_network_monitor_get_network_available(net_monitor)) {
        g_debug("Network is available");
        network_changed(net_monitor, TRUE, app);
    } else {
        g_debug("Network is NOT available");
        client_app_window_set_central_widget(app->main_window, "login");
        client_app_window_status(app->main_window, "Waiting for network connectivity...");
        client_app_window_set_central_widget_sensitive(app->main_window, FALSE);
        g_signal_connect(net_monitor, "network-changed", G_CALLBACK(network_changed), app);
    }
}

static void network_changed(GNetworkMonitor * net_monitor, gboolean network_available, gpointer user_data) {
    if (!network_available) return;
    ClientApp * app = CLIENT_APP(user_data);

    g_debug("Network is available NOW");
    g_signal_handlers_disconnect_by_func(net_monitor, G_CALLBACK(network_changed), app);
    if (client_conf_get_uri(app->conf) != NULL) {
        client_app_connect_with_spice_uri(app, client_conf_get_uri(app->conf));
        client_app_window_status(app->main_window, "Connecting to desktop...");
        client_app_window_set_central_widget(app->main_window, "login");
        client_app_window_set_central_widget_sensitive(app->main_window, FALSE);
    } else if (client_conf_get_host(app->conf) != NULL) {
        if (client_conf_get_username(app->conf) && client_conf_get_password(app->conf)) {
            app->autologin = TRUE;
        }
        client_app_show_login(app, NULL);
    } else
        client_app_configure(app, NULL);
}


/*
 * Open a URI that is provided in the command-line. Just save the first one
 * in the configuration object and call client_app_activate;
 */
static void client_app_open(GApplication * application, GFile ** files,
                            gint n_files, const gchar * hint) {
    ClientApp * app = CLIENT_APP(application);
    client_conf_set_uri(app->conf, g_file_get_uri(*files));
    client_app_activate(application);
}


static void client_app_request_desktop(ClientApp * app);

/*
 * Main window handlers: button pressed
 */
static void button_pressed_handler(ClientAppWindow * win, int button, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    switch (button) {
        case SETTINGS_BUTTON:
            client_app_configure(CLIENT_APP(user_data), NULL);
            break;
        case ABOUT_BUTTON:
            client_show_about(GTK_WINDOW(win), app->conf);
            break;
        case LOGIN_BUTTON:
            app->username = client_app_window_get_username(win);
            app->password = client_app_window_get_password(win);
            // Save the username in the config file
            client_conf_set_username(app->conf, app->username);
            client_conf_save(app->conf);
            client_app_request_desktop(CLIENT_APP(user_data));
            break;
        case SAVE_BUTTON:
            client_app_window_save_config(app->main_window, app->conf);
            client_conf_save(app->conf);
            // fallthrough
        default:  // BACK and DISCARD buttons
            client_app_show_login(app, NULL);
            break;
    }
}


/*
 * Main window handlers: key pressed; only F3 is meaningful.
 */
static gboolean key_event_handler(GtkWidget * widget, GdkEvent * event, gpointer user_data) {
    if (event->key.keyval == GDK_KEY_F3)
        client_app_configure(CLIENT_APP(user_data), NULL);
    return FALSE;
}


/*
 * Main window handlers: desktop selected (double-click, enter, connect button)
 */
static void desktop_selected_handler(ClientAppWindow * win, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);

    if (app->desktop_name) g_free(app->desktop_name);
    app->desktop_name = client_app_window_get_desktop(win);

    gchar * desktop = g_hash_table_lookup(app->desktops, app->desktop_name);
    if (desktop) {
        app->desktop = desktop;
        client_app_request_desktop(app);
    } else {
        g_warning("Selected desktop \"%s\" does not exist", app->desktop_name);
    }
}


/*
 * Window delete handler. It closes the VDI connection and all the remaining
 * windows, so that the application will exit as soon as the main loop is empty.
 * This handler is used for both the main window and the first Spice window.
 */
static gboolean delete_cb(GtkWidget * widget, GdkEvent * event, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    int i;

    if (app->connection)
        client_conn_disconnect(app->connection, CLIENT_CONN_DISCONNECT_USER);
    if (app->main_window != NULL && GTK_WIDGET(app->main_window) == widget) {
        app->main_window = NULL;
    } else {
        for (i = 0; i < MAX_WINDOWS; ++i) {
            if (GTK_WIDGET(app->windows[i]) == widget) {
                app->windows[i] = NULL;
            }
        }
    }

    return FALSE;
}


/*
 * Show the settings page. Cancel the current request if there is one.
 */
static void client_app_configure(ClientApp * app, const gchar * error) {
    client_app_window_load_config(app->main_window, app->conf);
    client_app_window_set_central_widget(app->main_window, "settings");
    client_app_window_set_central_widget_sensitive(app->main_window, TRUE);

    if (app->current_request) {
        client_request_cancel(app->current_request);
        g_clear_object(&app->current_request);
    }

    if (error != NULL) {
        client_app_window_error(app->main_window, error);
        app->autologin = FALSE;
    }
}


static void authmode_request_cb(ClientRequest * req, gpointer user_data);

/*
 * Show the login page, and start a new authmode request.
 */
static void client_app_show_login(ClientApp * app, const gchar * error) {
    client_app_window_load_config(app->main_window, app->conf);
    client_app_window_set_central_widget(app->main_window, "login");
    if (error == NULL)
        client_app_window_status(app->main_window, "Contacting server...");
    else {
        client_app_window_error(app->main_window, error);
        app->autologin = FALSE;
    }
    client_app_window_set_central_widget_sensitive(app->main_window, FALSE);

    app->username = app->password = app->desktop = "";

    g_clear_object(&app->current_request);
    g_autofree gchar * req_body = g_strdup_printf(
        "{\"hwaddress\": \"%s\"}", client_conf_get_terminal_id(app->conf));
    app->current_request = client_request_new_with_data(app->conf,
        "/vdi/authmode", req_body, authmode_request_cb, app);
}


/*
 * Authmode response handler.
 */
static void authmode_request_cb(ClientRequest * req, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    g_autoptr(GError) error = NULL;
    JsonNode * root = client_request_get_result(req, &error);

    if (error) {
        client_app_configure(app, "Failed to contact server");
        g_warning("Request failed: %s", error->message);

    } else if (JSON_NODE_HOLDS_OBJECT(root)) {
        JsonObject * response = json_node_get_object(root);
        const gchar * status = json_object_get_string_member(response, "status");
        const gchar * auth_mode = json_object_get_string_member(response, "auth_mode");

        if (g_strcmp0(status, "OK") != 0) {
            client_app_configure(app, "Access denied");
        } else if (g_strcmp0(auth_mode, "active_directory") == 0) {
            client_app_window_hide_status(app->main_window);
            client_app_window_set_central_widget_sensitive(app->main_window, TRUE);
            if (app->autologin)
                button_pressed_handler(app->main_window, LOGIN_BUTTON, app);
        } else {
            // Kiosk mode, make a desktop request
            client_app_request_desktop(app);
        }

    } else {
        client_app_configure(app, "Invalid response from server");
        g_warning("Invalid response from server, see debug messages");
    }
}


static void desktop_request_cb(ClientRequest * req, gpointer user_data);

/*
 * Start a new desktop request with currently selected username, password
 * and desktop name (which may be empty).
 */
static void client_app_request_desktop(ClientApp * app) {
    client_app_window_status(app->main_window, "Requesting desktop policy...");
    client_app_window_set_central_widget_sensitive(app->main_window, FALSE);

    g_clear_object(&app->current_request);
    g_autofree gchar * req_body = g_strdup_printf(
        "{\"hwaddress\": \"%s\", \"username\": \"%s\", \"password\": \"%s\", \"desktop\": \"%s\"}",
        client_conf_get_terminal_id(app->conf),
        app->username, app->password, app->desktop);
    app->current_request = client_request_new_with_data(app->conf,
        "/vdi/desktop", req_body, desktop_request_cb, app);
}


static gboolean client_app_repeat_request_desktop(gpointer user_data) {
    client_app_request_desktop(CLIENT_APP(user_data));
    return FALSE; // Cancel timeout
}


static void client_app_show_desktops(ClientApp * app, JsonObject * desktop);
static void client_app_connect_with_response(ClientApp * app, JsonObject * params);

/*
 * Desktop response handler.
 */
static void desktop_request_cb(ClientRequest * req, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    g_autoptr(GError) error = NULL;
    gboolean invalid = FALSE;
    JsonNode * root = client_request_get_result(req, &error);

    if (error) {
        client_app_show_login(app, "Failed to contact server");
        g_warning("Request failed: %s", error->message);

    } else if (JSON_NODE_HOLDS_OBJECT(root)) {
        JsonObject * response = json_node_get_object(root);
        const gchar * status = json_object_get_string_member(response, "status");
        if (g_strcmp0(status, "OK") == 0) {
            client_app_window_status(app->main_window, "Connecting to desktop...");
            client_app_connect_with_response(app, response);

        } else if (g_strcmp0(status, "Pending") == 0) {
            client_app_window_status(app->main_window, "Preparing desktop...");
            // Retry (forever) after 3 seconds
            g_timeout_add_seconds(3, client_app_repeat_request_desktop, app);

        } else if (g_strcmp0(status, "Error") == 0) {
            const gchar * message = json_object_get_string_member(response, "message");
            client_app_show_login(app, message);
            client_app_window_set_central_widget_sensitive(app->main_window, TRUE);

        } else if (g_strcmp0(status, "SelectDesktop") == 0) {
            const gchar * message = json_object_get_string_member(response, "message");
            g_autoptr(JsonParser) parser = json_parser_new_immutable();
            if (json_parser_load_from_data(parser, message, -1, NULL)) {
                root = json_parser_get_root(parser);
                if (JSON_NODE_HOLDS_OBJECT(root)) {
                    client_app_show_desktops(app, json_node_get_object(root));
                } else invalid = TRUE;
            } else invalid = TRUE;
        } else invalid = TRUE;
    } else invalid = TRUE;

    if (invalid) {
        client_app_show_login(app,
            "Invalid response from server");
        g_warning("Invalid response from server, see debug messages");
    }
}


/*
 * Show the desktops page. Fill in the list with the desktop response.
 */
static void client_app_show_desktops(ClientApp * app, JsonObject * desktops) {
    JsonObjectIter it;
    const gchar * desktop_key;
    JsonNode * desktop_node;

    g_hash_table_remove_all(app->desktops);
    json_object_iter_init(&it, desktops);
    while (json_object_iter_next(&it, &desktop_key, &desktop_node)) {
        const gchar * desktop_name = json_node_get_string(desktop_node);
        if (g_strcmp0(desktop_name, "") == 0) {
            desktop_name = desktop_key;
        }
        g_hash_table_insert(app->desktops, g_strdup(desktop_name), g_strdup(desktop_key));
    }

    g_autoptr(GList) desktop_names =
        g_list_sort(g_hash_table_get_keys(app->desktops), (GCompareFunc)g_strcmp0);
    const gchar * desktop = client_conf_get_desktop(app->conf);
    client_app_window_set_desktops(app->main_window, desktop_names, desktop);

    client_app_window_set_central_widget(app->main_window, "desktops");
    client_app_window_set_central_widget_sensitive(app->main_window, TRUE);
}


static void channel_new(SpiceSession * s, SpiceChannel * channel, gpointer user_data);
static void connection_disconnected(ClientConn * conn, ClientConnDisconnectReason reason,
                                    gpointer user_data);
void usb_connect_failed(GObject * object, SpiceUsbDevice * device,
                        GError * error, gpointer user_data);
static gboolean check_inactivity(gpointer user_data);

/*
 * Start the Spice connection with the current parameters, in the configuration object.
 * Also:
 * - connect to the USB manager signals if USB redirection is supported.
 * - start the inactivity timeout if it is set.
 */
static void client_app_connect(ClientApp * app) {
    SpiceSession * session = client_conn_get_session(app->connection);
    g_signal_connect(session, "channel-new",
                     G_CALLBACK(channel_new), app);
    g_signal_connect(app->connection, "disconnected",
                     G_CALLBACK(connection_disconnected), app);

    SpiceUsbDeviceManager * manager = spice_usb_device_manager_get(session, NULL);
    if (manager) {
        g_signal_connect(manager, "auto-connect-failed",
                         G_CALLBACK(usb_connect_failed), app);
        g_signal_connect(manager, "device-error",
                         G_CALLBACK(usb_connect_failed), app);
    }

    client_conn_connect(app->connection);
}

/*
 * Get connection parameters from the desktop response
 */
static void client_app_connect_with_response(ClientApp * app, JsonObject * params) {
    client_conf_get_options_from_response(app->conf, params);
    app->connection = client_conn_new(app->conf, params);
    SpiceSession * session = client_conn_get_session(app->connection);
    client_conf_set_gtk_session_options(app->conf, G_OBJECT(spice_gtk_session_get(session)));
    client_app_connect(app);
}

/*
 * Get connection parameters from the URI passed in the command line.
 */
static void client_app_connect_with_spice_uri(ClientApp * app, const gchar * uri) {
    app->connection = client_conn_new_with_uri(app->conf, uri);
    SpiceSession * session = client_conn_get_session(app->connection);
    client_conf_set_gtk_session_options(app->conf, G_OBJECT(spice_gtk_session_get(session)));
    client_app_connect(app);
}


static void main_channel_event(SpiceChannel * channel, SpiceChannelEvent event,
                               gpointer user_data);
static void display_monitors(SpiceChannel * display, GParamSpec * pspec, ClientApp * app);
static void main_agent_update(SpiceChannel * channel, ClientApp * app);

/*
 * New channel handler. Here, only these channels are useful:
 * - Main channel for obvious reasons.
 * - Display channel, to observe changes in monitors.
 * - Port channel, for flexVDI agent channel and serial ports.
 */
static void channel_new(SpiceSession * s, SpiceChannel * channel, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        g_debug("New main channel");
        app->main = SPICE_MAIN_CHANNEL(channel);
        g_signal_connect(channel, "channel-event",
                         G_CALLBACK(main_channel_event), app);
        g_signal_connect(channel, "main-agent-update",
                         G_CALLBACK(main_agent_update), app);
        main_agent_update(channel, app);
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        g_signal_connect(channel, "notify::monitors",
                         G_CALLBACK(display_monitors), app);
    }
}


static void client_app_close_windows(ClientApp * app) {
    GList * windows = gtk_application_get_windows(GTK_APPLICATION(app)),
        * window = windows, * next;
    for (; window != NULL; window = next) {
        next = window->next;
        gtk_widget_destroy(GTK_WIDGET(window->data));
    }
}


/*
 * Connection disconnected handler. If the connection fails at startup and the main
 * window is still showing, show an error message and return to login. Otherwise, show
 * a message dialog if the disconnection reason is other than NO_ERROR.
 */
static void connection_disconnected(ClientConn * conn, ClientConnDisconnectReason reason,
                                    gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);

    if (app->main_window) {
        client_app_show_login(app, "Failed to establish the connection, see the log file for further information.");
        client_app_window_set_central_widget_sensitive(app->main_window, TRUE);
    } else {
        if (reason != CLIENT_CONN_DISCONNECT_USER) {
            GtkMessageType type =
                reason == CLIENT_CONN_DISCONNECT_NO_ERROR || reason == CLIENT_CONN_DISCONNECT_INACTIVITY ?
                    GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR;
            g_autofree gchar * text = client_conn_get_reason_str(app->connection);
            g_warning("Closing app, reason %d: %s", reason, text);
            GtkWindow * active_window = gtk_application_get_active_window(GTK_APPLICATION(app));
            GtkWidget * dialog = gtk_message_dialog_new(active_window,
                GTK_DIALOG_MODAL, type, GTK_BUTTONS_CLOSE, "%s", text);
            gtk_window_set_title(GTK_WINDOW(dialog), "Connection closed");
            gtk_dialog_run(GTK_DIALOG (dialog));
            gtk_widget_destroy(dialog);
        }
        client_conf_save(app->conf);
        client_app_close_windows(app);
    }
}


/*
 * Main channel even handler. Mainly handles connection problems.
 */
static void main_channel_event(SpiceChannel * channel, SpiceChannelEvent event,
                               gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    const GError * error = NULL;

    switch (event) {
    case SPICE_CHANNEL_OPENED:
        g_debug("main channel: opened");
        break;
    case SPICE_CHANNEL_SWITCHING:
        g_debug("main channel: switching host");
        break;
    case SPICE_CHANNEL_CLOSED:
        g_debug("main channel: closed");
        client_conn_disconnect(app->connection, CLIENT_CONN_DISCONNECT_NO_ERROR);
        break;
    case SPICE_CHANNEL_ERROR_IO:
        client_conn_disconnect(app->connection, CLIENT_CONN_DISCONNECT_IO_ERROR);
        break;
    case SPICE_CHANNEL_ERROR_TLS:
    case SPICE_CHANNEL_ERROR_LINK:
    case SPICE_CHANNEL_ERROR_CONNECT:
        error = spice_channel_get_error(channel);
        g_debug("main channel: failed to connect");
        if (error) {
            g_debug("channel error: %s", error->message);
        }
        client_conn_disconnect(app->connection, CLIENT_CONN_DISCONNECT_CONN_ERROR);
        break;
    case SPICE_CHANNEL_ERROR_AUTH:
        g_warning("main channel: auth failure (wrong password?)");
        client_conn_disconnect(app->connection, CLIENT_CONN_DISCONNECT_AUTH_ERROR);
        break;
    default:
        g_warning("unknown main channel event: %u", event);
        break;
    }
}


static void spice_win_display_mark(SpiceChannel * channel, gint mark, SpiceWindow * win);
static void set_cp_sensitive(SpiceWindow * win, ClientApp * app);
static void user_activity_cb(SpiceWindow * win, ClientApp * app);

/*
 * Monitor changes handler. Creates a SpiceWindow for each new monitor.
 * Currently, multimonitor configurations are still not fully supported.
 */
static void display_monitors(SpiceChannel * display, GParamSpec * pspec, ClientApp * app) {
    GArray * monitors = NULL;
    int id;
    guint i;

    g_object_get(display,
                 "channel-id", &id,
                 "monitors", &monitors,
                 NULL);
    g_return_if_fail(monitors != NULL);
    g_return_if_fail(id == 0); // Only one display channel supported
    g_debug("Reported %d monitors in display channel %d", monitors->len, id);

    for (i = 0; i < monitors->len; ++i) {
        if (!app->windows[i]) {
            g_autofree gchar * name = NULL;
            if (app->desktop_name) {
                name = g_strdup(app->desktop_name);
            } else {
                g_object_get(client_conn_get_session(app->connection), "name", &name, NULL);
            }
            g_autofree gchar * title = g_strdup_printf("%s - flexVDI Client", name);
            SpiceWindow * win = spice_window_new(app->connection, display, app->conf, i, title);
            app->windows[i] = win;
            // Inform GTK that this is an application window
            gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(win));
            spice_g_signal_connect_object(display, "display-mark",
                                          G_CALLBACK(spice_win_display_mark), win, 0);
            if (i == 0) {
                g_signal_connect(win, "delete-event", G_CALLBACK(delete_cb), app);

                // Start the inactivity timeout when the first window is created
                gint inactivity_timeout = client_conf_get_inactivity_timeout(app->conf);
                if (inactivity_timeout >= 40) {
                    app->last_input_time = g_get_monotonic_time();
                    check_inactivity(app);
                }
            }
            g_signal_connect(win, "user-activity", G_CALLBACK(user_activity_cb), app);
            if (monitors->len == 1)
                gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
            gtk_widget_show_all(GTK_WIDGET(win));
            set_cp_sensitive(win, app);
            if (app->main_window) {
                gtk_widget_destroy(GTK_WIDGET(app->main_window));
                app->main_window = NULL;
                GSimpleAction * prefs_action =
                    G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "preferences"));
                g_simple_action_set_enabled(prefs_action, FALSE);
            }
        }
    }

    for (; i < MAX_WINDOWS; ++i) {
        if (!app->windows[i]) continue;
        gtk_widget_destroy(GTK_WIDGET(app->windows[i]));
        app->windows[i] = NULL;
        spice_main_channel_update_display_enabled(app->main, i, FALSE, TRUE);
        spice_main_channel_send_monitor_config(app->main);
    }

    g_clear_pointer(&monitors, g_array_unref);
}


static void spice_win_display_mark(SpiceChannel * channel, gint mark, SpiceWindow * win) {
    if (mark) {
        gtk_widget_show(GTK_WIDGET(win));
    } else {
        gtk_widget_hide(GTK_WIDGET(win));
    }
}


/*
 * Enable/disable copy&paste buttons when agent connects/disconnects
 */
static void set_cp_sensitive(SpiceWindow * win, ClientApp * app) {
    gboolean agent_connected;
    g_object_get(app->main, "agent-connected", &agent_connected, NULL);
    spice_win_set_cp_sensitive(win,
        agent_connected && !client_conf_get_disable_copy_from_guest(app->conf),
        agent_connected && !client_conf_get_disable_paste_to_guest(app->conf));
}


static void main_agent_update(SpiceChannel * channel, ClientApp * app) {
    int i;
    for (i = 0; i < MAX_WINDOWS; ++i) {
        if (app->windows[i]) {
            set_cp_sensitive(app->windows[i], app);
        }
    }
}


void usb_connect_failed(GObject * object, SpiceUsbDevice * device,
                        GError * error, gpointer user_data) {
    if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED)
        return;

    GtkWindow * parent = NULL;
    if (GTK_IS_WINDOW(user_data))
        parent = GTK_WINDOW(user_data);
    else
        parent = gtk_application_get_active_window(GTK_APPLICATION(user_data));
    GtkWidget * dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_CLOSE, "USB redirection error");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", error->message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}


/*
 * Save the current time as the last user activity time.
 */
static void user_activity_cb(SpiceWindow * win, ClientApp * app) {
    app->last_input_time = g_get_monotonic_time();
}


/*
 * Check user inactivity:
 * - If there are still more than 30 seconds left until timeout, program another
 *   check at that moment.
 * - If there are less than 30 seconds left, program another check every 100ms and
 *   show a notification reporting that the session is about to expire.
 * - If the timeout arrives, close the connection.
 */
static gboolean check_inactivity(gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    gint inactivity_timeout = client_conf_get_inactivity_timeout(app->conf);
    gint64 now = g_get_monotonic_time();
    gint time_to_inactivity = (app->last_input_time - now)/1000 + inactivity_timeout*1000;

    if (time_to_inactivity <= 0) {
        client_conn_disconnect(app->connection, CLIENT_CONN_DISCONNECT_INACTIVITY);
    } else if (time_to_inactivity <= 30000) {
        g_timeout_add(100, check_inactivity, app);
        GtkWindow * win = gtk_application_get_active_window(GTK_APPLICATION(app));
        if (win != NULL && SPICE_IS_WIN(win)) {
            int seconds = (time_to_inactivity + 999) / 1000;
            g_autofree gchar * text = g_strdup_printf(
                "Your session will end in %d seconds due to inactivity", seconds);
            spice_win_show_notification(SPICE_WIN(win), text, 200);
        }
    } else {
        g_timeout_add(time_to_inactivity - 30000, check_inactivity, app);
    }

    return FALSE;
}
