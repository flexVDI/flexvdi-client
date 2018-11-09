/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

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

#endif /* _SPICE_WIN_H */
