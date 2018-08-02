/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDI_PORT_H_
#define _FLEXVDI_PORT_H_

#include <glib.h>
#include <spice-client-gtk.h>
#include "flexdp.h"

// strnlen is not defined in MinGW32
#ifndef HAVE_STRNLEN
static __inline__ size_t strnlen(const char * start, size_t maxlen) {
    const char * stop = start;
    while (((stop - start) < maxlen) && *stop) ++stop;
    return stop - start;
}
#endif

uint8_t * flexvdi_port_get_msg_buffer(size_t size);
void flexvdi_port_delete_msg_buffer(uint8_t * buffer);
void flexvdi_port_send_msg(uint32_t type, uint8_t * buffer);
void flexvdi_port_send_msg_async(uint32_t type, uint8_t * buffer,
                      GAsyncReadyCallback callback, gpointer user_data);
void flexvdi_port_send_msg_finish(GObject * source_object, GAsyncResult * res, GError ** error);

void flexvdi_port_open(SpiceChannel * channel);
int flexvdi_is_agent_connected(void);
typedef void (*flexvdi_agent_connected_cb)(gpointer data);
void flexvdi_on_agent_connected(flexvdi_agent_connected_cb cb, gpointer data);
int flexvdi_agent_supports_capability(int cap);

// Print Client API
int flexvdi_get_printer_list(GSList ** printer_list);
int flexvdi_share_printer(const char * printer);
int flexvdi_unshare_printer(const char * printer);

#endif /* _FLEXVDI_PORT_H_ */
