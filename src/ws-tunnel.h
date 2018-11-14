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

#ifndef _WS_TUNNEL_H
#define _WS_TUNNEL_H

#include <glib-object.h>
#include <libsoup/soup.h>
#include <spice-client.h>


/*
 * WsTunnel
 *
 * A helper class to copy data from a GInputStream to a GOutputStream
 */
#define WS_TUNNEL_TYPE (ws_tunnel_get_type())
G_DECLARE_FINAL_TYPE(WsTunnel, ws_tunnel, WS, TUNNEL, GObject)

/*
 * ws_tunnel_new
 *
 * Create a new WebSocket tunnel for a spice channel
 */
WsTunnel * ws_tunnel_new(SpiceChannel * channel, SoupSession * soup, gchar * ws_uri);

/*
 * ws_tunnel_unref
 *
 * Close tunnel connections and unref the tunnel object
 */
void ws_tunnel_unref(WsTunnel * tunnel);

/*
 * ws_tunnel_is_channel
 *
 * Return whether this tunnel belongs to a certain channel. This is needed
 * because the open-fd signal may be received more than once while we
 * wait for the WS channel to open.
 */
gboolean ws_tunnel_is_channel(WsTunnel * tunnel, SpiceChannel * channel);

#endif /* _WS_TUNNEL_H */
