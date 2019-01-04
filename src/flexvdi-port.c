/*
    Copyright (C) 2014-2018 Flexible Software Solutions S.L.U.

    This file is part of flexVDI Client.

    flexVDI Client is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    flexVDI Client is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flexVDI Client. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "spice-client.h"
#define FLEXVDI_PROTO_IMPL
#include "flexdp.h"
#include "flexvdi-port.h"
#include "printclient-priv.h"

typedef enum {
    WAIT_NEW_MESSAGE,
    WAIT_DATA,
} WaitState;


struct _FlexvdiPort {
    GObject parent;
    SpicePortChannel * channel;
    gboolean opened;
    gchar * name;
    GCancellable * cancellable;
    WaitState state;
    FlexVDIMessageHeader current_header;
    uint8_t * buffer, * bufpos, * bufend;
    FlexVDICapabilitiesMsg agent_caps;
};

enum {
    FLEXVDI_PORT_AGENT_CONNECTED = 0,
    FLEXVDI_PORT_LAST_SIGNAL
};

static guint signals[FLEXVDI_PORT_LAST_SIGNAL];

G_DEFINE_TYPE(FlexvdiPort, flexvdi_port, G_TYPE_OBJECT);


static void flexvdi_port_dispose(GObject * obj);

static void flexvdi_port_class_init(FlexvdiPortClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->dispose = flexvdi_port_dispose;

    // Emited when the agent connects or disconnects
    signals[FLEXVDI_PORT_AGENT_CONNECTED] =
        g_signal_new("agent-connected",
                     FLEXVDI_PORT_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_INT);
}

static void flexvdi_port_init(FlexvdiPort * port) {
    port->cancellable = g_cancellable_new();
}


static void flexvdi_port_dispose(GObject * obj) {
    FlexvdiPort * port = FLEXVDI_PORT(obj);
    g_clear_object(&port->cancellable);
    g_clear_object(&port->channel);
    g_clear_pointer(&port->name, g_free);
    G_OBJECT_CLASS(flexvdi_port_parent_class)->dispose(obj);
}


FlexvdiPort * flexvdi_port_new() {
    return g_object_new(FLEXVDI_PORT_TYPE, NULL);
}


static void flexvdi_port_opened(FlexvdiPort * port);
static void flexvdi_port_data(FlexvdiPort * port, gpointer data, int size);
static void flexvdi_port_channel_event(SpiceChannel * channel, int event, FlexvdiPort * port);

void flexvdi_port_set_channel(FlexvdiPort * port, SpicePortChannel * channel) {
    port->channel = g_object_ref(channel);
    g_object_get(channel, "port-name", &port->name, NULL);
    g_signal_connect_swapped(channel, "notify::port-opened",
                             G_CALLBACK(flexvdi_port_opened), port);
    g_signal_connect_swapped(channel, "port-data",
                             G_CALLBACK(flexvdi_port_data), port);
    g_signal_connect(channel, "channel-event",
                     G_CALLBACK(flexvdi_port_channel_event), port);
    flexvdi_port_opened(port);
}


static void flexvdi_port_channel_event(SpiceChannel * channel, int event, FlexvdiPort * port) {
    if (SPICE_IS_PORT_CHANNEL(channel) &&
        port->channel == SPICE_PORT_CHANNEL(channel) &&
        event == SPICE_CHANNEL_CLOSED) {
        g_clear_pointer(&port->name, g_free);
        g_clear_object(&port->channel);
    }
}


static const size_t HEADER_SIZE = sizeof(FlexVDIMessageHeader);

uint8_t * flexvdi_port_get_msg_buffer(size_t size) {
    uint8_t * buf = (uint8_t *)g_malloc(size + HEADER_SIZE);
    if (buf) {
        ((FlexVDIMessageHeader *)buf)->size = size;
        return buf + HEADER_SIZE;
    } else
        return NULL;
}


void flexvdi_port_delete_msg_buffer(uint8_t * buffer) {
    g_free(buffer - HEADER_SIZE);
}


/*
 * send_message_cb
 *
 * Async callback of a simple send operation. Checks for error and releases the message buffer
 */
static void send_message_cb(GObject * source_object, GAsyncResult * res, gpointer user_data) {
    GError * error = NULL;
    FlexvdiPort * port = FLEXVDI_PORT(source_object);
    flexvdi_port_send_msg_finish(port, res, &error);
    if (error != NULL)
        g_warning("Port %s: Error sending message, %s", port->name, error->message);
    g_clear_error(&error);
    flexvdi_port_delete_msg_buffer(user_data);
}


/*
 * send_message_async_cb
 *
 * Async callback of a send operation. Checks for cancellation and calls the user-supplied
 * callback.
 */
static void send_message_async_cb(GObject * source_object, GAsyncResult * res, gpointer user_data) {
    GTask * task = user_data;
    GError * error = NULL;
    spice_port_channel_write_finish(SPICE_PORT_CHANNEL(source_object), res, &error);
    if (error) {
        g_task_return_error(task, error);
    } else {
        g_task_return_pointer(task, NULL, NULL);
    }
    g_object_unref(task);
}


void flexvdi_port_send_msg(FlexvdiPort * port, uint32_t type, uint8_t * buffer) {
    flexvdi_port_send_msg_async(port, type, buffer, send_message_cb, buffer);
}


void flexvdi_port_send_msg_async(FlexvdiPort * port, uint32_t type, uint8_t * buffer,
                                 GAsyncReadyCallback callback, gpointer user_data) {
    GTask * task = g_task_new(port, port->cancellable, callback, user_data);
    FlexVDIMessageHeader * head = (FlexVDIMessageHeader *)(buffer - HEADER_SIZE);
    size_t size = head->size + HEADER_SIZE;
    g_debug("Port %s: sending message type %d, size %d", port->name, (int)type, (int)size);
    head->type = type;
    marshallMessage(type, buffer, head->size);
    marshallHeader(head);
    spice_port_channel_write_async(port->channel, head, size, port->cancellable,
                                   send_message_async_cb, task);
}


void flexvdi_port_send_msg_finish(FlexvdiPort * port, GAsyncResult * res, GError ** error) {
    g_task_propagate_pointer(G_TASK(res), error);
}


static void prepare_port_buffer(FlexvdiPort * port, size_t size) {
    g_free(port->buffer);
    port->buffer = port->bufpos = (uint8_t *)g_malloc(size);
    port->bufend = port->bufpos + size;
}


/*
 * Handler for the port-opened channel property.
 */
static void flexvdi_port_opened(FlexvdiPort * port) {
    gboolean opened = FALSE;
    g_object_get(port->channel, "port-opened", &opened, NULL);
    if (port->opened == opened) return; // Do nothing if the state did not change
    port->opened = opened;

    if (opened) {
        g_info("Port %s: flexVDI agent is connected", port->name);
        memset(port->agent_caps.caps, 0, sizeof(port->agent_caps));
        prepare_port_buffer(port, sizeof(FlexVDIMessageHeader));
        port->state = WAIT_NEW_MESSAGE;

        // Send RESET and CAPABILITIES messages
        uint8_t * buf = flexvdi_port_get_msg_buffer(sizeof(FlexVDIResetMsg));
        if (buf) {
            flexvdi_port_send_msg(port, FLEXVDI_RESET, buf);
        }
        buf = flexvdi_port_get_msg_buffer(sizeof(FlexVDICapabilitiesMsg));
        if (buf) {
            FlexVDICapabilitiesMsg * capMsg = (FlexVDICapabilitiesMsg *)buf;
            capMsg->caps[0] = capMsg->caps[1] = capMsg->caps[2] = capMsg->caps[3] = 0;
            setCapability(capMsg, FLEXVDI_CAP_PRINTING);
            setCapability(capMsg, FLEXVDI_CAP_POWEREVENT);
            flexvdi_port_send_msg(port, FLEXVDI_CAPABILITIES, buf);
        }

    } else {
        g_info("Port %s: flexVDI agent is disconnected", port->name);
        g_cancellable_cancel(port->cancellable);
        g_object_unref(port->cancellable);
        port->cancellable = g_cancellable_new();
        g_signal_emit(port, signals[FLEXVDI_PORT_AGENT_CONNECTED], 0, FALSE);
    }
}


int flexvdi_port_agent_supports_capability(FlexvdiPort * port, int cap) {
    return supportsCapability(&port->agent_caps, cap);
}


/*
 * handle_capabilities_msg
 *
 * Handle the CAPABILITIES message comming from the agent
 */
static void handle_capabilities_msg(FlexvdiPort * port, FlexVDICapabilitiesMsg * msg) {
    memcpy(&port->agent_caps, msg, sizeof(FlexVDICapabilitiesMsg));
    g_debug("Port %s:flexVDI agent capabilities: %08x %08x %08x %08x", port->name,
            port->agent_caps.caps[3], port->agent_caps.caps[2],
            port->agent_caps.caps[1], port->agent_caps.caps[0]);
    g_signal_emit(port, signals[FLEXVDI_PORT_AGENT_CONNECTED], 0, TRUE);
}


/*
 * handle_message
 *
 * Handle messages comming from the agent
 */
static void handle_message(FlexvdiPort * port) {
    switch(port->current_header.type) {
    case FLEXVDI_CAPABILITIES:
        handle_capabilities_msg(port, (FlexVDICapabilitiesMsg *)port->buffer);
        break;
    case FLEXVDI_PRINTJOB:
        handle_print_job((FlexVDIPrintJobMsg *)port->buffer);
        break;
    case FLEXVDI_PRINTJOBDATA:
        handle_print_job_data((FlexVDIPrintJobDataMsg *)port->buffer);
        break;
    }
}


/*
 * prepare_one_byte
 *
 * This function discards the first byte of the next header and prepares to read
 * another byte at the end. It is called when the header information makes no sense,
 * and we try to find the next coherent header one byte at a time.
 */
static void prepare_one_byte(FlexvdiPort * port) {
    size_t i;
    uint8_t * p = port->buffer;
    for (i = 0; i < sizeof(FlexVDIMessageHeader) - 1; ++i)
        p[i] = p[i + 1];
    port->bufpos = &p[i];
}


/*
 * port_data
 *
 * Read data arriving from the port channel into the prepared buffer. Do not
 * expect data arriving one message at a time.
 */
static void flexvdi_port_data(FlexvdiPort * port, gpointer data, int size) {
    void * end = data + size;

    while (data < end || port->buffer == port->bufend) { // Special case: read 0 bytes
        // Fill the buffer
        size = end - data;
        if (port->bufend - port->bufpos < size)
            size = port->bufend - port->bufpos;
        memcpy(port->bufpos, data, size);
        port->bufpos += size;
        data += size;

        if (port->bufpos < port->bufend) return; // Data consumed, buffer not filled

        switch (port->state) {
        case WAIT_NEW_MESSAGE:
            // We were waiting for the header of a new message
            port->current_header = *((FlexVDIMessageHeader *)port->buffer);
            unmarshallHeader(&port->current_header);
            // Check header consistency
            if (port->current_header.size > FLEXVDI_MAX_MESSAGE_LENGTH) {
                g_warning("Port %s: Oversized message (%u > %u)", port->name,
                          port->current_header.size, FLEXVDI_MAX_MESSAGE_LENGTH);
                prepare_one_byte(port);
            } else if (port->current_header.type >= FLEXVDI_MAX_MESSAGE_TYPE) {
                g_warning("Port %s: Unknown message type %d", port->name, port->current_header.type);
                prepare_one_byte(port);
            } else {
                prepare_port_buffer(port, port->current_header.size);
                port->state = WAIT_DATA;
            }
            break;

        case WAIT_DATA:
            // We were waiting for the data of the message
            g_debug("Port %s: Received message type %u, size %u", port->name,
                    port->current_header.type, port->current_header.size);
            if (!unmarshallMessage(port->current_header.type, port->buffer, port->current_header.size)) {
                g_warning("Port %s: Wrong message size on reception (%u)", port->name,
                          port->current_header.size);
            } else {
                handle_message(port);
            }
            prepare_port_buffer(port, sizeof(FlexVDIMessageHeader));
            port->state = WAIT_NEW_MESSAGE;
            break;
        }
    }
}


int flexvdi_port_is_agent_connected(FlexvdiPort * port) {
    return port->opened;
}
