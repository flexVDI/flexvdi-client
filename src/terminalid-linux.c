/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <glib.h>

gchar * discover_terminal_id() {
    struct ifconf ifc;
    struct ifreq ifr[10];
    ifc.ifc_len = sizeof(ifr);
    ifc.ifc_req = ifr;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock != -1 && ioctl(sock, SIOCGIFCONF, &ifc) != -1 && ifc.ifc_len > 0) {
        for (int i = 0; i < ifc.ifc_len / sizeof(struct ifreq); ++i) {
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
