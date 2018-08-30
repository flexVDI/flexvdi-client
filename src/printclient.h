/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _PRINTCLIENT_H_
#define _PRINTCLIENT_H_

#include <stddef.h>
#include <glib.h>
#include "flexdp.h"

typedef struct PrintJob {
    int file_handle;
    char * name;
    char * options;
} PrintJob;

void init_print_client();
void handle_print_job(FlexVDIPrintJobMsg * msg);
void handle_print_job_data(FlexVDIPrintJobDataMsg * msg);
char * get_ppd_file(const char * printer);
void print_job(PrintJob * job);
char * get_job_options(char * options, const char * opName);

int flexvdi_get_printer_list(GSList ** printerList);
int flexvdi_share_printer(const char * printer);
int flexvdi_unshare_printer(const char * printer);

#endif /* _PRINTCLIENT_H_ */
