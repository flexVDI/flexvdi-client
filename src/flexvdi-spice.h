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

uint8_t * getMsgBuffer(size_t size);
void deleteMsgBuffer(uint8_t * buffer);
void sendMessage(uint32_t type, uint8_t * buffer);
void sendMessageAsync(uint32_t type, uint8_t * buffer,
                      GAsyncReadyCallback callback, gpointer user_data);
void sendMessageFinish(GObject * source_object, GAsyncResult * res, GError ** error);

#endif /* _FLEXVDI_SPICE_H_ */
