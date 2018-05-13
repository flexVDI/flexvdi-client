#ifndef _FLEXVDICLIENTWIN_H
#define _FLEXVDICLIENTWIN_H

#include <gtk/gtk.h>
#include "flexvdi-client-app.h"


#define CLIENT_APP_WINDOW_TYPE (client_app_window_get_type ())
G_DECLARE_FINAL_TYPE(ClientAppWindow, client_app_window, CLIENT, APP_WINDOW, GtkApplicationWindow)

ClientAppWindow * client_app_window_new(ClientApp * app);


#endif /* _FLEXVDICLIENTWIN_H */
