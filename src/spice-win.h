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

#ifndef _SPICE_WIN_H
#define _SPICE_WIN_H

#include <glib-object.h>
#include <spice-client-gtk.h>
#include "client-conn.h"
#include "configuration.h"


#define SPICE_WIN_TYPE (spice_window_get_type())
G_DECLARE_FINAL_TYPE(SpiceWindow, spice_window, SPICE, WIN, GtkApplicationWindow)

SpiceWindow * spice_window_new(ClientConn * conn, SpiceChannel * channel,
                               ClientConf * conf, int id, gchar * title);
void spice_win_set_cp_sensitive(SpiceWindow * win, gboolean copy, gboolean paste);
void spice_win_show_notification(SpiceWindow * win, const gchar * text, gint duration);
void spice_win_release_mouse_pointer(SpiceWindow * win);
int spice_window_get_monitor(SpiceWindow * win);

#endif /* _SPICE_WIN_H */
