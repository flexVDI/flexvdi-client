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

#include <gtk/gtk.h>
#include "client-app.h"
#include "client-log.h"


int main (int argc, char * argv[]) {
    // Check the --version option as early as possible
    int i;
    for (i = 0; i < argc; ++i) {
        if (g_strcmp0(argv[i], "--version") == 0) {
            printf("flexVDI Client v" VERSION_STRING "\n"
                   "Copyright (C) 2018 Flexible Software Solutions S.L.\n");
            return 0;
        }
    }

    client_log_setup(argc, argv);
    g_message("Starting flexVDI Client v" VERSION_STRING);

    return g_application_run(G_APPLICATION(client_app_new()), argc, argv);
}
