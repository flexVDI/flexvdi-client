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

#ifndef _PRINTCLIENT_H_
#define _PRINTCLIENT_H_

#include <glib.h>
#include <glib-object.h>
#include "flexvdi-port.h"

/*
 * PrintJobManager
 *
 * Manages print jobs arriving from the guest.
 */
#define PRINT_JOB_MANAGER_TYPE (print_job_manager_get_type())
G_DECLARE_FINAL_TYPE(PrintJobManager, print_job_manager, PRINT, JOB_MANAGER, GObject)

PrintJobManager * print_job_manager_new();

gboolean print_job_manager_handle_message(
    PrintJobManager * pjb, uint32_t type, gpointer data);

int flexvdi_get_printer_list(GSList ** printerList);
int flexvdi_share_printer(FlexvdiPort * port, const char * printer);
int flexvdi_unshare_printer(FlexvdiPort * port, const char * printer);

#endif /* _PRINTCLIENT_H_ */
