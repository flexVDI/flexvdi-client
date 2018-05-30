#include <stdio.h>
#include <gtk/gtk.h>

#include "client-app.h"
#include "configuration.h"
#include "client-win.h"
#include "client-request.h"

struct _ClientApp {
    GtkApplication parent;
    ClientConf * conf;
    ClientAppWindow * main_window;
    ClientRequest * current_request;
};

G_DEFINE_TYPE(ClientApp, client_app, GTK_TYPE_APPLICATION);

static void client_app_configure(ClientApp * app) {
    client_app_window_set_status(app->main_window,
        "Please, provide the manager's address (and port, if it is not 443)");
    client_app_window_set_central_widget(app->main_window, "settings");
    client_app_window_set_central_widget_sensitive(app->main_window, TRUE);
    if (app->current_request) {
        client_request_cancel(app->current_request);
        g_clear_object(&app->current_request);
    }
}

static void authmode_request_cb(ClientRequest * req, gpointer user_data) {
    ClientApp * app = CLIENT_APP(user_data);
    client_app_window_set_central_widget_sensitive(app->main_window, TRUE);
    g_autoptr(GError) error = NULL;
    JsonNode * root = client_request_get_result(req, &error);
    if (error) {
        g_warning("Request failed: %s", error->message);
    }
}

static void client_app_show_login(ClientApp * app) {
    client_app_window_set_status(app->main_window,
        "Fill in your credentials");
    client_app_window_set_central_widget(app->main_window, "login");
    client_app_window_set_central_widget_sensitive(app->main_window, FALSE);
    g_clear_object(&app->current_request);
    g_autofree gchar * req_body = g_strdup_printf(
        "{\"hwaddress\": \"%s\"}", client_conf_get_terminal_id(app->conf));
    app->current_request = client_request_new_with_data(app->conf,
        "/vdi/authmode", req_body, authmode_request_cb, app);
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

static void config_button_pressed_handler(ClientAppWindow * win, gpointer user_data) {
    client_app_configure(CLIENT_APP(user_data));
}

static void login_button_pressed_handler(ClientAppWindow * win, gpointer user_data) {
    printf("Login\n");
}

static void client_app_init(ClientApp * app) {
    app->conf = client_conf_new();
    g_application_add_main_option_entries(G_APPLICATION(app),
        client_conf_get_cmdline_entries(app->conf));
    g_signal_connect(app, "handle-local-options",
        G_CALLBACK(client_app_handle_options), NULL);
}

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
    g_signal_connect(app->main_window, "login-button-pressed",
        G_CALLBACK(login_button_pressed_handler), NULL);

    if (client_conf_get_host(app->conf) != NULL)
        client_app_show_login(app);
    else
        client_app_configure(app);
}

static void client_app_class_init(ClientAppClass * class) {
    G_APPLICATION_CLASS(class)->activate = client_app_activate;
}

ClientApp * client_app_new(void) {
    return g_object_new(CLIENT_APP_TYPE,
                        "application-id", "com.flexvdi.client",
                        "flags", G_APPLICATION_NON_UNIQUE,
                        NULL);
}
