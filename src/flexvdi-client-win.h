#ifndef _FLEXVDICLIENTWIN_H
#define _FLEXVDICLIENTWIN_H

#include <gtk/gtk.h>
#include "flexvdi-client-app.h"
#include "configuration.h"


#define CLIENT_APP_WINDOW_TYPE (client_app_window_get_type ())
G_DECLARE_FINAL_TYPE(ClientAppWindow, client_app_window, CLIENT, APP_WINDOW, GtkApplicationWindow)

ClientAppWindow * client_app_window_new(ClientApp * app);
void client_app_window_set_config(ClientAppWindow * win, ClientConf * conf);
void client_app_window_set_info(ClientAppWindow * win, const gchar * text);
void client_app_window_set_status(ClientAppWindow * win, const gchar * text);
void client_app_window_set_central_widget(ClientAppWindow * win, const gchar * name);
void client_app_window_set_central_widget_sensitive(ClientAppWindow * win, gboolean sensitive);


#endif /* _FLEXVDICLIENTWIN_H */
