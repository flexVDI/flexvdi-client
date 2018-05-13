#include <gtk/gtk.h>

#include "flexvdi-client-app.h"
#include "flexvdi-client-win.h"

struct _ClientApp {
    GtkApplication parent;
};

G_DEFINE_TYPE(ClientApp, client_app, GTK_TYPE_APPLICATION);

static void client_app_init(ClientApp * app) {}

static void client_app_activate(GApplication *app) {
    ClientAppWindow * win = client_app_window_new(CLIENT_APP(app));
    gtk_window_present(GTK_WINDOW(win));
}

static void client_app_class_init(ClientAppClass * class) {
    G_APPLICATION_CLASS(class)->activate = client_app_activate;
}

ClientApp * client_app_new(void) {
    return g_object_new (CLIENT_APP_TYPE,
                        "application-id", "com.flexvdi.client",
                        "flags", G_APPLICATION_NON_UNIQUE,
                        NULL);
}
