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
