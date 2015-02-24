/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDI_SPICE_H_
#define _FLEXVDI_SPICE_H_

#include <stddef.h>
#include "FlexVDIProto.h"

typedef struct PrintJob {
    gint fileHandle;
    gchar * name;
    gchar * options;
} PrintJob;

void initPrintClient();

void handlePrintJob(FlexVDIPrintJobMsg * msg);

void handlePrintJobData(FlexVDIPrintJobDataMsg * msg);

char * getPPDFile(const char * printer);

void printJob(PrintJob * job);

#endif /* _FLEXVDI_SPICE_H_ */
