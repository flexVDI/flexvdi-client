/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _PRINTCLIENT_H_
#define _PRINTCLIENT_H_

#include <stddef.h>
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

#endif /* _PRINTCLIENT_H_ */
