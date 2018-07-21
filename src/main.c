#include <gtk/gtk.h>
#include <gst/gst.h>
#include "client-app.h"

int main (int argc, char * argv[]) {
    gst_init(&argc, &argv);
    return g_application_run(G_APPLICATION(client_app_new()), argc, argv);
}
