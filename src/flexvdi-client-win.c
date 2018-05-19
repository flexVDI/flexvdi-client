#include <gtk/gtk.h>

#include "flexvdi-client-app.h"
#include "flexvdi-client-win.h"

struct _ClientAppWindow {
    GtkApplicationWindow parent;
    GtkLabel * version;
    GtkLabel * info;
};

G_DEFINE_TYPE(ClientAppWindow, client_app_window, GTK_TYPE_APPLICATION_WINDOW);

static void client_app_window_init(ClientAppWindow * win) {
    GtkCssProvider * css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css_provider, "/com/flexvdi/client/style.css");
    GdkScreen * screen = gtk_widget_get_screen(GTK_WIDGET(win));
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
    gtk_widget_init_template(GTK_WIDGET(win));

    gtk_label_set_text(win->version, "version " VERSION_STRING);
}

static void client_app_window_class_init(ClientAppWindowClass * class) {
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                                "/com/flexvdi/client/window.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, version);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, info);
}

void client_app_window_set_info(ClientAppWindow * win, const gchar * text) {
    gtk_label_set_text(win->info, text);
}

ClientAppWindow * client_app_window_new(ClientApp * app) {
    return g_object_new(CLIENT_APP_WINDOW_TYPE, "application", app, NULL);
}
