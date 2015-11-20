/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDI_SPICE_H_
#define _FLEXVDI_SPICE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <glib.h>
#include <gio/gio.h>

// strnlen is not defined in MinGW32
#ifndef HAVE_STRNLEN
static __inline__ size_t strnlen(const char * start, size_t maxlen) {
    const char * stop = start;
    while (((stop - start) < maxlen) && *stop) ++stop;
    return stop - start;
}
#endif

typedef enum {
    L_DEBUG,
    L_INFO,
    L_WARN,
    L_ERROR
} FlexVDILogLevel;

void flexvdiLog(FlexVDILogLevel level, const char * format, ...);
uint8_t * getMsgBuffer(size_t size);
void deleteMsgBuffer(uint8_t * buffer);
void sendMessage(uint32_t type, uint8_t * buffer);
void sendMessageAsync(uint32_t type, uint8_t * buffer,
                      GAsyncReadyCallback callback, gpointer user_data);
void sendMessageFinish(GObject * source_object, GAsyncResult * res, GError ** error);

// SSO API
void flexvdiSpiceSendCredentials(const char * username, const char * password,
                                const char * domain);

#ifdef WITH_PRINTING
// Print Client API
int flexvdiSpiceGetPrinterList(GSList ** printerList);
int flexvdiSpiceSharePrinter(const char * printer);
int flexvdiSpiceUnsharePrinter(const char * printer);
#endif

#endif /* _FLEXVDI_SPICE_H_ */
