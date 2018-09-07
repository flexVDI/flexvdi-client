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


/*
 * flexvdi_port_get_msg_buffer
 *
 * Get a buffer for a message of a certain size. The allocated memory includes the
 * message header, and the returned pointer points to the message area. Destroy the
 * buffer with flexvdi_port_delete_msg_buffer.
 */
uint8_t * flexvdi_port_get_msg_buffer(size_t size);

/*
 * flexvdi_port_delete_msg_buffer
 *
 * Deletes a message buffer allocated with flexvdi_port_get_msg_buffer
 */
void flexvdi_port_delete_msg_buffer(uint8_t * buffer);

/*
 * flexvdi_port_send_msg
 *
 * Sends a message through the flexVDI port of a certain type. The operation is
 * asynchronous, with a "fire and forget" semantic. The buffer is automatically released.
 */
void flexvdi_port_send_msg(uint32_t type, uint8_t * buffer);

/*
 * flexvdi_port_send_msg_async
 *
 * Sends a message through the flexVDI port of a certain type, asynchronously, with
 * a callback for when the operation ends.
 */
void flexvdi_port_send_msg_async(uint32_t type, uint8_t * buffer,
                      GAsyncReadyCallback callback, gpointer user_data);

/*
 * flexvdi_port_send_msg_finish
 *
 * Ends an asynchronous send operation.
 */
void flexvdi_port_send_msg_finish(GObject * source_object, GAsyncResult * res, GError ** error);

/*
 * flexvdi_port_open
 *
 * Opens the flexVDI port on a certain channel. The channel must be a SpicePortChannel
 * with the name "es.flexvdi.guest_agent"
 */
void flexvdi_port_open(SpiceChannel * channel);

/*
 * flexvdi_is_agent_connected
 *
 * Checks whether the flexVDI guest agent is connected
 */
int flexvdi_is_agent_connected(void);

/*
 * flexvdi_on_agent_connected
 *
 * Calls a callback when the guest agent connects.
 */
typedef void (*flexvdi_agent_connected_cb)(gpointer data);
void flexvdi_on_agent_connected(flexvdi_agent_connected_cb cb, gpointer data);

/*
 * flexvdi_agent_supports_capability
 *
 * Checks whether the guest agent supports a certain flexDP capability
 */
int flexvdi_agent_supports_capability(int cap);

#endif /* _FLEXVDI_PORT_H_ */
