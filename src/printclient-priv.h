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

#ifndef _PRINTCLIENT_PRIV_H_
#define _PRINTCLIENT_PRIV_H_

#include "flexdp.h"

typedef struct PrintJob {
    int file_handle;
    char * name;
    char * options;
} PrintJob;

void handle_print_job(FlexVDIPrintJobMsg * msg);
void handle_print_job_data(FlexVDIPrintJobDataMsg * msg);
char * get_ppd_file(const char * printer);
int print_job(PrintJob * job);
char * get_job_options(char * options, const char * opName);

#endif /* _PRINTCLIENT_PRIV_H_ */
