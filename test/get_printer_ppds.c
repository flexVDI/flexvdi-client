/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdio.h>
#include <glib.h>
#include "client/printclient.h"
#include "client/flexvdi-spice.h"


int main(int argc, char * argv[]) {
    GSList * printers, * i;
    flexvdiSpiceGetPrinterList(&printers);
    for (i = printers; i != NULL; i = g_slist_next(i)) {
        char * fileName = getPPDFile((const char *)i->data);
        printf("Printer %s: %s\n", (const char *)i->data, fileName);
        g_free(fileName);
    }
    g_slist_free_full(printers, g_free);
    return 0;
}
