#include "about.h"

void client_show_about(GtkWindow * parent, ClientConf * conf) {
    GdkPixbuf * logo = gdk_pixbuf_new_from_resource("/com/flexvdi/client/images/logo-client.png", NULL);
    g_autofree gchar * version = g_strconcat("v", VERSION_STRING, NULL);
    g_autofree gchar * desc = g_strconcat("Terminal ID: ",
        client_conf_get_terminal_id(conf), NULL);
    gtk_show_about_dialog(parent,
        "title", "About flexVDI Client",
        "logo", logo,
        "program-name", "flexVDI Client",
        "version", version,
        "comments", desc,
        "copyright", "© 2018 Flexible Software Solutions S.L.U.",
        "license-type", GTK_LICENSE_GPL_3_0,
        "website", "https://flexvdi.com",
        "website-label", "https://flexvdi.com",
        NULL);
}
