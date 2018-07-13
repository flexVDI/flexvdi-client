#include <stdio.h>
#include <gtk/gtk.h>
#include <spice-client-gtk.h>

#include "client-app.h"
#include "configuration.h"
#include "client-win.h"
#include "client-request.h"
#include "client-conn.h"
#include "spice-win.h"

#define MAX_WINDOWS 16

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
};

G_DEFINE_TYPE(ClientApp, client_app, GTK_TYPE_APPLICATION);

static void client_app_activate(GApplication * gapp);

static void client_app_class_init(ClientAppClass * class) {
    G_APPLICATION_CLASS(class)->activate = client_app_activate;
}

static gint client_app_handle_options(GApplication * gapp, GVariantDict * opts, gpointer u) {
    ClientApp * app = CLIENT_APP(gapp);
    if (client_conf_show_version(app->conf)) {
        printf("flexVDI Client v" VERSION_STRING "\n"
               "Copyright (C) 2018 Flexible Software Solutions S.L.\n");
        return 0;
    }

    return -1;
}

static void client_app_init(ClientApp * app) {
    app->conf = client_conf_new();
    app->username = app->password = app->desktop = "";
    app->desktops = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_application_add_main_option_entries(G_APPLICATION(app),
        client_conf_get_cmdline_entries(app->conf));
    g_application_add_option_group(G_APPLICATION(app), spice_get_option_group());
    g_signal_connect(app, "handle-local-options",
        G_CALLBACK(client_app_handle_options), NULL);
}

ClientApp * client_app_new(void) {
    return g_object_new(CLIENT_APP_TYPE,
                        "application-id", "com.flexvdi.client",
                        "flags", G_APPLICATION_NON_UNIQUE,
                        NULL);
}

static void config_button_pressed_handler(ClientAppWindow * win, gpointer user_data);
static gboolean key_event_handler(GtkWidget * widget, GdkEvent * event, gpointer user_data);
static void save_button_pressed_handler(ClientAppWindow * win, gpointer user_data);
static void login_button_pressed_handler(ClientAppWindow * win, gpointer user_data);
static void desktop_selected_handler(ClientAppWindow * win, gpointer user_data);
static gboolean delete_cb(GtkWidget * widget, GdkEvent * event, gpointer user_data);

static void client_app_configure(ClientApp * app);
static void client_app_show_login(ClientApp * app);

static void client_app_activate(GApplication * gapp) {
    ClientApp * app = CLIENT_APP(gapp);
    app->main_window = client_app_window_new(app);
    gtk_window_present(GTK_WINDOW(app->main_window));

    const gchar * tid = client_conf_get_terminal_id(app->conf);
    g_autofree gchar * text = g_strconcat("Terminal ID: ", tid, NULL);
    client_app_window_set_info(app->main_window, text);

    client_app_window_set_config(app->main_window, app->conf);
    g_signal_connect(app->main_window, "config-button-pressed",
        G_CALLBACK(config_button_pressed_handler), app);
    g_signal_connect(app->main_window, "key-press-event",
        G_CALLBACK(key_event_handler), app);
    g_signal_connect(app->main_window, "save-button-pressed",
        G_CALLBACK(save_button_pressed_handler), app);
    g_signal_connect(app->main_window, "login-button-pressed",
        G_CALLBACK(login_button_pressed_handler), app);
    g_signal_connect(app->main_window, "desktop-selected",
        G_CALLBACK(desktop_selected_handler), app);
    g_signal_connect(app->main_window, "delete-event",
        G_CALLBACK(delete_cb), app);

    if (client_conf_get_host(app->conf) != NULL)
        client_app_show_login(app);
    else
        client_app_configure(app);
}

static void config_button_pressed_handler(ClientAppWindow * win, gpointer user_data) {
    client_app_configure(CLIENT_APP(user_data));
}

static gboolean key_event_handler(GtkWidget * widget, GdkEvent * event, gpointer user_data) {
    if (event->key.keyval == GDK_KEY_F3)
        client_app_configure(CLIENT_APP(user_data));
    return FALSE;
}

static void save_button_pressed_handler(ClientAppWindow * win, gpointer user_data) {
    client_app_show_login(CLIENT_APP(user_data));
}

static void client_app_request_desktop(ClientApp * app);

static void login_button_pressed_handler(ClientAppWindow * win, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    app->username = client_app_window_get_username(win);
    app->password = client_app_window_get_password(win);
    client_app_request_desktop(CLIENT_APP(user_data));
}

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

static gboolean delete_cb(GtkWidget * widget, GdkEvent * event, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    client_conn_disconnect(app->connection, CLIENT_CONN_DISCONNECT_NO_ERROR);
    return FALSE;
}

static void client_app_configure(ClientApp * app) {
    client_app_window_set_status(app->main_window, FALSE,
        "Please, provide the manager's address (and port, if it is not 443)");
    client_app_window_set_central_widget(app->main_window, "settings");
    client_app_window_set_central_widget_sensitive(app->main_window, TRUE);
    if (app->current_request) {
        client_request_cancel(app->current_request);
        g_clear_object(&app->current_request);
    }
}

static void authmode_request_cb(ClientRequest * req, gpointer user_data);

static void client_app_show_login(ClientApp * app) {
    client_app_window_set_status(app->main_window, FALSE,
        "Contacting server...");
    client_app_window_set_central_widget(app->main_window, "login");
    client_app_window_set_central_widget_sensitive(app->main_window, FALSE);
    app->username = app->password = app->desktop = "";
    g_clear_object(&app->current_request);
    g_autofree gchar * req_body = g_strdup_printf(
        "{\"hwaddress\": \"%s\"}", client_conf_get_terminal_id(app->conf));
    app->current_request = client_request_new_with_data(app->conf,
        "/vdi/authmode", req_body, authmode_request_cb, app);
}

static void authmode_request_cb(ClientRequest * req, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    g_autoptr(GError) error = NULL;
    JsonNode * root = client_request_get_result(req, &error);
    if (error) {
        client_app_window_set_status(app->main_window, TRUE,
            "Failed to contact server");
        g_warning("Request failed: %s", error->message);
    } else if (JSON_NODE_HOLDS_OBJECT(root)) {
        JsonObject * response = json_node_get_object(root);
        const gchar * status = json_object_get_string_member(response, "status");
        const gchar * auth_mode = json_object_get_string_member(response, "auth_mode");
        if (g_strcmp0(status, "OK") != 0) {
            client_app_window_set_status(app->main_window, TRUE,
                "Access denied");
        } else if (g_strcmp0(auth_mode, "active_directory") == 0) {
            client_app_window_set_status(app->main_window, FALSE,
                "Fill in your credentials");
            client_app_window_set_central_widget_sensitive(app->main_window, TRUE);
        } else {
            client_app_request_desktop(app);
        }
    } else {
        client_app_window_set_status(app->main_window, TRUE,
            "Invalid response from server");
        g_warning("Invalid response from server, see debug messages");
    }
}

static void desktop_request_cb(ClientRequest * req, gpointer user_data);

static void client_app_request_desktop(ClientApp * app) {
    client_app_window_set_status(app->main_window, FALSE,
        "Requesting desktop policy...");
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
static void client_app_connect(ClientApp * app, JsonObject * params);

static void desktop_request_cb(ClientRequest * req, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    g_autoptr(GError) error = NULL;
    gboolean invalid = FALSE;
    JsonNode * root = client_request_get_result(req, &error);
    if (error) {
        client_app_window_set_status(app->main_window, TRUE,
            "Failed to contact server");
        g_warning("Request failed: %s", error->message);
    } else if (JSON_NODE_HOLDS_OBJECT(root)) {
        JsonObject * response = json_node_get_object(root);
        const gchar * status = json_object_get_string_member(response, "status");
        if (g_strcmp0(status, "OK") == 0) {
            client_app_connect(app, response);
        } else if (g_strcmp0(status, "Pending") == 0) {
            client_app_window_set_status(app->main_window, FALSE,
                "Preparing desktop...");
            g_timeout_add_seconds(3, client_app_repeat_request_desktop, app);
        } else if (g_strcmp0(status, "Error") == 0) {
            const gchar * message = json_object_get_string_member(response, "message");
            client_app_window_set_status(app->main_window, TRUE, message);
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
        client_app_window_set_status(app->main_window, TRUE,
            "Invalid response from server");
        g_warning("Invalid response from server, see debug messages");
    }
}

static void client_app_show_desktops(ClientApp * app, JsonObject * desktops) {
    g_hash_table_remove_all(app->desktops);
    JsonObjectIter it;
    const gchar * desktop_key;
    JsonNode * desktop_node;
    json_object_iter_init(&it, desktops);
    while (json_object_iter_next(&it, &desktop_key, &desktop_node))
        g_hash_table_insert(app->desktops,
            g_strdup(json_node_get_string(desktop_node)), g_strdup(desktop_key));
    g_autoptr(GList) desktop_names =
        g_list_sort(g_hash_table_get_keys(app->desktops), (GCompareFunc)g_strcmp0);
    client_app_window_set_desktops(app->main_window, desktop_names);

    client_app_window_set_status(app->main_window, FALSE,
        "Select your desktop");
    client_app_window_set_central_widget(app->main_window, "desktops");
    client_app_window_set_central_widget_sensitive(app->main_window, TRUE);
}

static void channel_new(SpiceSession * s, SpiceChannel * channel, gpointer user_data);

static void client_app_connect(ClientApp * app, JsonObject * params) {
    app->connection = client_conn_new(app->conf, params);

    SpiceSession * session = client_conn_get_session(app->connection);
    g_signal_connect(session, "channel-new",
                     G_CALLBACK(channel_new), app);

    client_conn_connect(app->connection);
}

static void main_channel_event(SpiceChannel * channel, SpiceChannelEvent event,
                               gpointer user_data);
static void display_monitors(SpiceChannel * display, GParamSpec * pspec, ClientApp * app);

static void channel_new(SpiceSession * s, SpiceChannel * channel, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        g_debug("New main channel");
        app->main = SPICE_MAIN_CHANNEL(channel);
         g_signal_connect(channel, "channel-event",
                          G_CALLBACK(main_channel_event), app);
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        g_signal_connect(channel, "notify::monitors",
                         G_CALLBACK(display_monitors), app);
    }

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
        // g_signal_connect(channel, "inputs-modifiers",
        //                  G_CALLBACK(inputs_modifiers), app);
    }

    if (SPICE_IS_USBREDIR_CHANNEL(channel)) {
        // Support usb redir
    }

    if (SPICE_IS_PORT_CHANNEL(channel)) {
        // Check flexvdi port channel
    }
}

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
        client_conn_disconnect(app->connection, CLIENT_CONN_ISCONNECT_IO_ERROR);
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
            g_autofree gchar * title = g_strdup_printf("%s #%d", app->desktop_name, i);
            SpiceWindow * win = spice_window_new(app->connection, display, app->conf, i, title);
            app->windows[i] = win;
            gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(win));
            spice_g_signal_connect_object(display, "display-mark",
                                          G_CALLBACK(spice_win_display_mark), win, 0);
            if (i == 0)
                g_signal_connect(win, "delete-event", G_CALLBACK(delete_cb), app);
            if (monitors->len == 1)
                gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER_ALWAYS);
            gtk_widget_show(GTK_WIDGET(win));
            if (app->main_window) {
                gtk_widget_destroy(GTK_WIDGET(app->main_window));
                app->main_window = NULL;
            }
        }
    }

    for (; i < MAX_WINDOWS; ++i) {
        if (!app->windows[i]) continue;
        gtk_widget_destroy(GTK_WIDGET(app->windows[i]));
        app->windows[i] = NULL;
        spice_main_set_display_enabled(app->main, i, FALSE);
        spice_main_send_monitor_config(app->main);
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
