/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDI_SPICE_H_
#define _FLEXVDI_SPICE_H_

#include <stddef.h>
#include <glib.h>

typedef struct FlexVDISpiceCallbacks {
    void * (* getBuffer)(size_t size);
    void (* sendMessage)(void * buffer, size_t size);
} FlexVDISpiceCallbacks;

void flexvdiSpiceInit(FlexVDISpiceCallbacks * callbacks);

int flexvdiSpiceData(void * data, size_t size);

// SSO API
int flexvdiSpiceSendCredentials(const char * username, const char * password,
                                const char * domain);

// Print Client API
int flexvdiSpiceGetPrinterList(GSList ** printerList);
int flexvdiSpiceSharePrinter(const char * printer);
int flexvdiSpiceUnsharePrinter(const char * printer);

#endif /* _FLEXVDI_SPICE_H_ */
