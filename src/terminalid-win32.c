/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include <windows.h>
#include <iphlpapi.h>
#include <glib.h>

gchar * discover_terminal_id() {
    DWORD dwBufLen = 0;
    gchar * result = NULL;
    GetAdaptersInfo(NULL, &dwBufLen);
    PIP_ADAPTER_INFO adapters = g_malloc(dwBufLen), it;
    if (GetAdaptersInfo(adapters, &dwBufLen) == ERROR_SUCCESS) {
        for (it = adapters; it != NULL; it = it->Next) {
            if (it->AddressLength >= 6) {
                result = g_strdup_printf("%02x:%02x:%02x:%02x:%02x:%02x",
                    it->Address[0], it->Address[1], it->Address[2],
                    it->Address[3], it->Address[4], it->Address[5]);
                break;
            }
        }
        g_free(adapters);
    }
    return result ? result : g_strdup("");
}
