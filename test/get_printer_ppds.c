/*
    Copyright (C) 2014-2018 Flexible Software Solutions S.L.U.

    This file is part of flexVDI Client.

    flexVDI Client is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    flexVDI Client is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flexVDI Client. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <glib.h>
#include "src/printclient.h"
#include "src/flexvdi-port.h"


int main(int argc, char * argv[]) {
    GSList * printers, * i;
    flexvdi_get_printer_list(&printers);
    for (i = printers; i != NULL; i = g_slist_next(i)) {
        char * fileName = get_ppd_file((const char *)i->data);
        printf("Printer %s: %s\n", (const char *)i->data, fileName);
        g_free(fileName);
    }
    g_slist_free_full(printers, g_free);
    return 0;
}
