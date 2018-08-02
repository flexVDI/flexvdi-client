/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "spice-client.h"
#define FLEXVDI_PROTO_IMPL
#include "flexdp.h"
#include "flexvdi-port.h"

typedef enum {
    WAIT_NEW_MESSAGE,
    WAIT_DATA,
} WaitState;


typedef struct FlexvdiPort {
    SpicePortChannel * channel;
    gboolean connected;
    GCancellable * cancellable;
    WaitState state;
    FlexVDIMessageHeader current_header;
    uint8_t * buffer, * bufpos, * bufend;
    FlexVDICapabilitiesMsg agent_caps;
} FlexvdiPort;


static FlexvdiPort port;


typedef struct ConnectionHandler {
    flexvdi_agent_connected_cb cb;
    gpointer data;
} ConnectionHandler;


static GSList * connection_handlers;


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


typedef struct AsyncUserData {
    GAsyncReadyCallback callback;
    gpointer user_data;
} AsyncUserData;


static gpointer new_async_user_data(GAsyncReadyCallback cb, gpointer u) {
    AsyncUserData * aud = g_malloc(sizeof(AsyncUserData));
    if (aud) {
        aud->callback = cb;
        aud->user_data = u;
    }
    return aud;
}


static void send_message_cb(GObject * source_object, GAsyncResult * res, gpointer user_data) {
    GError * error = NULL;
    flexvdi_port_send_msg_finish(source_object, res, &error);
    if (error != NULL)
        g_warning("Error sending message, %s", error->message);
    g_clear_error(&error);
    flexvdi_port_delete_msg_buffer(user_data);
}


static void send_message_async_cb(GObject * source_object, GAsyncResult * res,
                                  gpointer user_data) {
    AsyncUserData * aud = (AsyncUserData *)user_data;
    if (!g_cancellable_is_cancelled(port.cancellable)) {
        aud->callback(source_object, res, aud->user_data);
    }
    g_free(aud);
}


void flexvdi_port_send_msg(uint32_t type, uint8_t * buffer) {
    flexvdi_port_send_msg_async(type, buffer, send_message_cb, buffer);
}


void flexvdi_port_send_msg_async(uint32_t type, uint8_t * buffer,
                                 GAsyncReadyCallback callback, gpointer user_data) {
    FlexVDIMessageHeader * head = (FlexVDIMessageHeader *)(buffer - HEADER_SIZE);
    size_t size = head->size + HEADER_SIZE;
    g_debug("Sending message type %d, size %d", (int)type, (int)size);
    head->type = type;
    marshallMessage(type, buffer, head->size);
    marshallHeader(head);
    spice_port_channel_write_async(port.channel, head, size, port.cancellable, send_message_async_cb,
                                   new_async_user_data(callback, user_data));
}


void flexvdi_port_send_msg_finish(GObject * source_object, GAsyncResult * res, GError ** error) {
    spice_port_channel_write_finish(SPICE_PORT_CHANNEL(source_object), res, error);
}


static void prepare_port_buffer(size_t size) {
    g_free(port.buffer);
    port.buffer = port.bufpos = (uint8_t *)g_malloc(size);
    port.bufend = port.bufpos + size;
}


static void port_data(SpicePortChannel * pchannel, gpointer data, int size);

void flexvdi_port_open(SpiceChannel * channel) {
    gboolean opened = FALSE;
    g_object_get(channel, "port-opened", &opened, NULL);
    if (opened) {
        g_info("flexVDI guest agent connected");
        g_signal_connect(channel, "port-data", G_CALLBACK(port_data), NULL);
        port.connected = TRUE;
        port.channel = SPICE_PORT_CHANNEL(channel);
        port.cancellable = g_cancellable_new();
        port.buffer = NULL;
        memset(port.agent_caps.caps, 0, sizeof(port.agent_caps));
        prepare_port_buffer(sizeof(FlexVDIMessageHeader));
        port.state = WAIT_NEW_MESSAGE;
        uint8_t * buf = flexvdi_port_get_msg_buffer(sizeof(FlexVDIResetMsg));
        if (buf) {
            flexvdi_port_send_msg(FLEXVDI_RESET, buf);
        }
        buf = flexvdi_port_get_msg_buffer(sizeof(FlexVDICapabilitiesMsg));
        if (buf) {
            FlexVDICapabilitiesMsg * capMsg = (FlexVDICapabilitiesMsg *)buf;
            capMsg->caps[0] = capMsg->caps[1] = capMsg->caps[2] = capMsg->caps[3] = 0;
            setCapability(capMsg, FLEXVDI_CAP_PRINTING);
            flexvdi_port_send_msg(FLEXVDI_CAPABILITIES, buf);
        }
    } else {
        g_info("flexVDI guest agent disconnected");
        g_signal_handlers_disconnect_by_func(channel, G_CALLBACK(port_data), NULL);
        port.connected = FALSE;
        port.channel = NULL;
        g_cancellable_cancel(port.cancellable);
        g_object_unref(port.cancellable);
        port.cancellable = NULL;
    }
}


int flexvdi_agent_supports_capability(int cap) {
    return supportsCapability(&port.agent_caps, cap);
}


static void handle_capabilities_msg(FlexVDICapabilitiesMsg * msg) {
    memcpy(&port.agent_caps, msg, sizeof(FlexVDICapabilitiesMsg));
    g_debug("flexVDI guest agent capabilities: %08x %08x %08x %08x",
            port.agent_caps.caps[3], port.agent_caps.caps[2],
            port.agent_caps.caps[1], port.agent_caps.caps[0]);
    GSList * it;
    for (it = connection_handlers; it != NULL; it = g_slist_next(it)) {
        ConnectionHandler * handler = (ConnectionHandler *)it->data;
        handler->cb(handler->data);
    }
}


static void handle_message(uint32_t type, uint8_t * msg) {
    switch(type) {
    case FLEXVDI_CAPABILITIES:
        handle_capabilities_msg((FlexVDICapabilitiesMsg *)msg);
        break;
    case FLEXVDI_PRINTJOB:
        handlePrintJob((FlexVDIPrintJobMsg *)msg);
        break;
    case FLEXVDI_PRINTJOBDATA:
        handlePrintJobData((FlexVDIPrintJobDataMsg *)msg);
        break;
    }
}


static void prepare_one_byte() {
    size_t i;
    uint8_t * p = port.buffer;
    for (i = 0; i < sizeof(FlexVDIMessageHeader) - 1; ++i)
        p[i] = p[i + 1];
    port.bufpos = &p[i];
}


static void port_data(SpicePortChannel * pchannel, gpointer data, int size) {
    void * end = data + size;
    while (data < end || port.buffer == port.bufend) { // Special case: read 0 bytes
        // Fill the buffer
        size = end - data;
        if (port.bufend - port.bufpos < size)
            size = port.bufend - port.bufpos;
        memcpy(port.bufpos, data, size);
        port.bufpos += size;
        data += size;
        if (port.bufpos < port.bufend) return; // Data consumed, buffer not filled
        switch (port.state) {
        case WAIT_NEW_MESSAGE:
            port.current_header = *((FlexVDIMessageHeader *)port.buffer);
            unmarshallHeader(&port.current_header);
            if (port.current_header.size > FLEXVDI_MAX_MESSAGE_LENGTH) {
                g_warning("Oversized message (%u > %u)",
                           port.current_header.size, FLEXVDI_MAX_MESSAGE_LENGTH);
                prepare_one_byte();
            } else if (port.current_header.type >= FLEXVDI_MAX_MESSAGE_TYPE) {
                g_warning("Unknown message type %d", port.current_header.type);
                prepare_one_byte();
            } else {
                prepare_port_buffer(port.current_header.size);
                port.state = WAIT_DATA;
            }
            break;
        case WAIT_DATA:
            g_debug("Received message type %u, size %u",
                       port.current_header.type, port.current_header.size);
            if (!unmarshallMessage(port.current_header.type, port.buffer, port.current_header.size)) {
                g_warning("Wrong message size on reception (%u)",
                           port.current_header.size);
            } else {
                handle_message(port.current_header.type, port.buffer);
            }
            prepare_port_buffer(sizeof(FlexVDIMessageHeader));
            port.state = WAIT_NEW_MESSAGE;
            break;
        }
    }
}


int flexvdi_is_agent_connected(void) {
    return port.connected;
}


void flexvdi_on_agent_connected(flexvdi_agent_connected_cb cb, gpointer data) {
    ConnectionHandler * h = g_malloc(sizeof(ConnectionHandler));
    h->cb = cb;
    h->data = data;
    connection_handlers = g_slist_prepend(connection_handlers, h);
}
