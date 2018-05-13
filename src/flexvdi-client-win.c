#include <gtk/gtk.h>

#include "flexvdi-client-app.h"
#include "flexvdi-client-win.h"

struct _ClientAppWindow {
    GtkApplicationWindow parent;
};

G_DEFINE_TYPE(ClientAppWindow, client_app_window, GTK_TYPE_APPLICATION_WINDOW);

static void client_app_window_init(ClientAppWindow * app) {}

static void client_app_window_class_init(ClientAppWindowClass * class) {}

ClientAppWindow * client_app_window_new(ClientApp * app) {
    return g_object_new(CLIENT_APP_WINDOW_TYPE, "application", app, NULL);
}
