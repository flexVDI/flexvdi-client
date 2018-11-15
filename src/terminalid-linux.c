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
#include <glib.h>

gchar * discover_terminal_id() {
    int i;
    struct ifconf ifc;
    struct ifreq ifr[10];
    ifc.ifc_len = sizeof(ifr);
    ifc.ifc_req = ifr;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock != -1 && ioctl(sock, SIOCGIFCONF, &ifc) != -1 && ifc.ifc_len > 0) {
        for (i = 0; i < ifc.ifc_len / sizeof(struct ifreq); ++i) {
            if (ioctl(sock, SIOCGIFFLAGS, &ifr[i]) == 0
                && !(ifr[i].ifr_flags & IFF_LOOPBACK) // don't count loopback
                && ioctl(sock, SIOCGIFHWADDR, &ifr[i]) == 0) {
                close(sock);
                unsigned char * mac = (unsigned char *)ifr[i].ifr_hwaddr.sa_data;
                return g_strdup_printf("%02x:%02x:%02x:%02x:%02x:%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            }
        }
        close(sock);
    }

    return g_strdup("");
}
