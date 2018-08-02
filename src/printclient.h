/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _PRINTCLIENT_H_
#define _PRINTCLIENT_H_

#include <stddef.h>
#include <glib.h>
#include "flexdp.h"

typedef struct PrintJob {
    int fileHandle;
    char * name;
    char * options;
} PrintJob;

void initPrintClient();
void handlePrintJob(FlexVDIPrintJobMsg * msg);
void handlePrintJobData(FlexVDIPrintJobDataMsg * msg);
char * getPPDFile(const char * printer);
void printJob(PrintJob * job);
char * getJobOption(char * options, const char * opName);

int flexvdi_get_printer_list(GSList ** printerList);
int flexvdi_share_printer(const char * printer);
int flexvdi_unshare_printer(const char * printer);

#endif /* _PRINTCLIENT_H_ */
