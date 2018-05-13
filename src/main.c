#include <gtk/gtk.h>
#include "flexvdi-client-app.h"

int main (int argc, char * argv[]) {
    return g_application_run(G_APPLICATION(client_app_new()), argc, argv);
}
