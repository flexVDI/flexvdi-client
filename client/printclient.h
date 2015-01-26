/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDI_SPICE_H_
#define _FLEXVDI_SPICE_H_

#include <stddef.h>
#include "FlexVDIProto.h"

void initPrintClient();

void handlePrintJob(FlexVDIPrintJobMsg * msg);

void handlePrintJobData(FlexVDIPrintJobDataMsg * msg);

#endif /* _FLEXVDI_SPICE_H_ */
