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
G_DECLARE_FINAL_TYPE(SpiceWindow, spice_window, SPICE, WIN, GtkWindow)

SpiceWindow * spice_window_new(ClientConn * conn, SpiceChannel * channel,
                               ClientConf * conf, int id, gchar * title);


#endif /* _SPICE_WIN_H */
