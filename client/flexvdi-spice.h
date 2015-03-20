/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDI_SPICE_H_
#define _FLEXVDI_SPICE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <glib.h>

typedef enum {
    L_DEBUG,
    L_INFO,
    L_WARN,
    L_ERROR
} FlexVDILogLevel;

void flexvdiLog(FlexVDILogLevel level, const char * format, ...);
uint8_t * getMsgBuffer(size_t size);
void sendMessage(uint32_t type, uint8_t * buffer);

// SSO API
void flexvdiSpiceSendCredentials(const char * username, const char * password,
                                 const char * domain);

// Print Client API
void flexvdiSpiceGetPrinterList(GSList ** printerList);
void flexvdiSpiceSharePrinter(const char * printer);
void flexvdiSpiceUnsharePrinter(const char * printer);

#endif /* _FLEXVDI_SPICE_H_ */
