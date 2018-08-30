/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include <windows.h>
#include <iphlpapi.h>
#include <glib.h>

gchar * discover_terminal_id() {
    DWORD buf_len = 0;
    GetAdaptersInfo(NULL, &buf_len);
    g_autofree PIP_ADAPTER_INFO adapters = g_malloc(buf_len), it;
    if (GetAdaptersInfo(adapters, &buf_len) == ERROR_SUCCESS) {
        for (it = adapters; it != NULL; it = it->Next) {
            if (it->AddressLength >= 6) {
                return g_strdup_printf("%02x:%02x:%02x:%02x:%02x:%02x",
                    it->Address[0], it->Address[1], it->Address[2],
                    it->Address[3], it->Address[4], it->Address[5]);
            }
        }
    }
    return g_strdup("");
}
