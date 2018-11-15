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
