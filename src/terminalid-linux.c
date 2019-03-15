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

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <glib.h>

#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "flexvdi-tid"

gchar * discover_terminal_id() {
    g_debug("Discovering terminal ID");
    gchar * id = NULL;
    struct ifreq ifr;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct if_nameindex * if_ni = if_nameindex(), * i;
    g_debug("socket returned %d, if_nameindex returned %p", sock, if_ni);
    if (sock != -1 && if_ni != NULL) {
        for (i = if_ni; i->if_index != 0 && i->if_name != NULL; ++i) {
            g_debug("Checking interface %s", i->if_name);
            memcpy(ifr.ifr_name, i->if_name, strlen(i->if_name) + 1);
            if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
                g_debug("SIOCGIFFLAGS ioctl failed: %s", strerror(errno));
            } else if (ifr.ifr_flags & IFF_LOOPBACK) {
                g_debug("Skipping the loopback device");
            } else if (ioctl(sock, SIOCGIFHWADDR, &ifr) != 0) {
                g_debug("SIOCGIFHWADDR ioctl failed: %s", strerror(errno));
            } else {
                close(sock);
                unsigned char * mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
                id = g_strdup_printf("%02x:%02x:%02x:%02x:%02x:%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                g_debug("Its MAC address is %s", id);
                break;
            }
        }
    }

    close(sock);
    if_freenameindex(if_ni);
    return id ? id : g_strdup("");
}
