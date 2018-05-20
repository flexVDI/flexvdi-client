#include <stdio.h>
#include <gtk/gtk.h>

#include "flexvdi-client-app.h"
#include "configuration.h"
#include "flexvdi-client-win.h"

gchar * discover_terminal_id();

struct _ClientApp {
    GtkApplication parent;
    ClientConf * conf;
    ClientAppWindow * main_window;
};

G_DEFINE_TYPE(ClientApp, client_app, GTK_TYPE_APPLICATION);

static void client_app_configure(ClientApp * app) {
    client_app_window_set_status(app->main_window,
        "Please, provide the manager's address (and port, if it is not 443)");
    client_app_window_set_central_widget(app->main_window, "settings");
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
    g_application_add_main_option_entries(G_APPLICATION(app),
        client_conf_get_cmdline_entries(app->conf));
    g_signal_connect(app, "handle-local-options",
        G_CALLBACK(client_app_handle_options), NULL);
}

static void client_app_activate(GApplication *gapp) {
    ClientApp * app = CLIENT_APP(gapp);
    app->main_window = client_app_window_new(app);
    gtk_window_present(GTK_WINDOW(app->main_window));

    gchar * tid = discover_terminal_id();
    if (tid[0] == '\0') {
        // TODO: Random terminal id
    }
    gchar * text = g_strconcat("Terminal ID: ", tid, NULL);
    client_app_window_set_info(app->main_window, text);
    g_free(tid);
    g_free(text);

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
