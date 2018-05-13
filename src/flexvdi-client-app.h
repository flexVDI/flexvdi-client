#ifndef _FLEXVDICLIENTAPP_H
#define _FLEXVDICLIENTAPP_H

#include <gtk/gtk.h>


#define CLIENT_APP_TYPE (client_app_get_type())
G_DECLARE_FINAL_TYPE(ClientApp, client_app, CLIENT, APP, GtkApplication)

ClientApp * client_app_new(void);


#endif /* _FLEXVDICLIENTAPP_H */
