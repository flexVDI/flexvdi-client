/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "flexvdi-spice.h"
#include "flexvdi-port.h"
#include "flexvdi-cmdline.h"
#include "spice-client.h"
#define FLEXVDI_PROTO_IMPL
#include "FlexVDIProto.h"
#include "printclient.h"
#include "serialredir.h"


typedef struct flexvdi_port {
    SpicePortChannel * channel;
    GCancellable * cancellable;
} flexvdi_port;


static flexvdi_port port;


void flexvdiLog(FlexVDILogLevel level, const char * format, ...) {
    va_list args;
    va_start(args, format);
    GLogLevelFlags map[] = {
        G_LOG_LEVEL_DEBUG,
        G_LOG_LEVEL_INFO,
        G_LOG_LEVEL_WARNING,
        G_LOG_LEVEL_ERROR,
        G_LOG_LEVEL_CRITICAL
    };
    g_logv(G_LOG_DOMAIN, map[level], format, args);
    va_end(args);
}


static const size_t HEADER_SIZE = sizeof(FlexVDIMessageHeader);

uint8_t * getMsgBuffer(size_t size) {
    uint8_t * buf = (uint8_t *)g_malloc(size + HEADER_SIZE);
    if (buf) {
        ((FlexVDIMessageHeader *)buf)->size = size;
        return buf + HEADER_SIZE;
    } else
        return NULL;
}


void deleteMsgBuffer(uint8_t * buffer) {
    g_free(buffer - HEADER_SIZE);
}


typedef struct AsyncUserData {
    GAsyncReadyCallback callback;
    gpointer user_data;
} AsyncUserData;


static gpointer newAsyncUserData(GAsyncReadyCallback cb, gpointer u) {
    AsyncUserData * aud = g_malloc(sizeof(AsyncUserData));
    if (aud) {
        aud->callback = cb;
        aud->user_data = u;
    }
    return aud;
}


static void sendMessageCb(GObject * source_object, GAsyncResult * res, gpointer user_data) {
    GError * error = NULL;
    sendMessageFinish(source_object, res, &error);
    if (error != NULL)
        flexvdiLog(L_WARN, "Error sending message, %s", error->message);
    g_clear_error(&error);
    deleteMsgBuffer(user_data);
}


static void sendMessageAsyncCb(GObject * source_object, GAsyncResult * res,
                               gpointer user_data) {
    AsyncUserData * aud = (AsyncUserData *)user_data;
    if (!g_cancellable_is_cancelled(port.cancellable)) {
        aud->callback(source_object, res, aud->user_data);
    }
    g_free(aud);
}


void sendMessage(uint32_t type, uint8_t * buffer) {
    sendMessageAsync(type, buffer, sendMessageCb, buffer);
}


void sendMessageAsync(uint32_t type, uint8_t * buffer,
                      GAsyncReadyCallback callback, gpointer user_data) {
    FlexVDIMessageHeader * head = (FlexVDIMessageHeader *)(buffer - HEADER_SIZE);
    size_t size = head->size + HEADER_SIZE;
    flexvdiLog(L_DEBUG, "Sending message type %d, size %d", (int)type, (int)size);
    head->type = type;
    marshallMessage(type, buffer, head->size);
    marshallHeader(head);
    spice_port_write_async(port.channel, head, size, port.cancellable, sendMessageAsyncCb,
                           newAsyncUserData(callback, user_data));
}


void sendMessageFinish(GObject * source_object, GAsyncResult * res, GError ** error) {
    spice_port_write_finish(SPICE_PORT_CHANNEL(source_object), res, error);
}


static void port_opened(SpiceChannel *channel, GParamSpec *pspec) {
    gchar *name = NULL;
    gboolean opened = FALSE;
    g_object_get(channel, "port-name", &name, "port-opened", &opened, NULL);
    if (g_strcmp0(name, "es.flexvdi.guest_agent") == 0) {
        if (opened) {
            flexvdiLog(L_INFO, "flexVDI guest agent connected\n");
            port.channel = SPICE_PORT_CHANNEL(channel);
            port.cancellable = g_cancellable_new();
            uint8_t * buf = getMsgBuffer(0);
            if (buf) {
                sendMessage(FLEXVDI_RESET, buf);
            }
            const gchar * username = getUsernameOption(),
                    * password = getPasswordOption(),
                    * domain = getDomainOption();
            if (username && password) {
                flexvdi_send_credentials(username, password, domain ? domain : "");
            }
        }
#ifndef WIN32
    } else {
        serialPortOpened(channel);
#endif
    }
}


static void handleMessage(uint32_t type, uint8_t * msg) {
    switch(type) {
    case FLEXVDI_PRINTJOB:
        handlePrintJob((FlexVDIPrintJobMsg *)msg);
        break;
    case FLEXVDI_PRINTJOBDATA:
        handlePrintJobData((FlexVDIPrintJobDataMsg *)msg);
        break;
    }
}


static void port_data(SpicePortChannel * pchannel, gpointer data, int size) {
    if (pchannel != port.channel) {
#ifndef WIN32
        serialPortData(pchannel, data, size);
#endif
        return;
    }

    static enum {
        WAIT_NEW_MESSAGE,
        WAIT_DATA,
    } state = WAIT_NEW_MESSAGE;
    static FlexVDIMessageHeader curHeader;
    static uint8_t * buffer, * bufpos, * bufend;

    void * end = data + size;
    while (data < end) {
        switch (state) {
        case WAIT_NEW_MESSAGE:
            // Assume headers arrive at once
            curHeader = *((FlexVDIMessageHeader *)data);
            marshallHeader(&curHeader);
            data += sizeof(FlexVDIMessageHeader);
            buffer = bufpos = (uint8_t *)malloc(curHeader.size);
            bufend = bufpos + curHeader.size;
            state = WAIT_DATA;
            break;
        case WAIT_DATA:
            size = end - data;
            if (bufend - bufpos < size) size = bufend - bufpos;
            memcpy(bufpos, data, size);
            bufpos += size;
            data += size;
            if (bufpos == bufend) {
                if (!marshallMessage(curHeader.type, buffer, curHeader.size)) {
                    flexvdiLog(L_WARN, "Wrong message size on reception");
                    return;
                }
                handleMessage(curHeader.type, buffer);
                free(buffer);
                state = WAIT_NEW_MESSAGE;
            }
            break;
        }
    }
}


static void channel_new(SpiceSession * s, SpiceChannel * channel) {
    if (SPICE_IS_PORT_CHANNEL(channel)) {
        g_signal_connect(channel, "notify::port-opened",
                         G_CALLBACK(port_opened), NULL);
        g_signal_connect(channel, "port-data",
                         G_CALLBACK(port_data), NULL);
    }
}


static void channel_destroy(SpiceSession * s, SpiceChannel * channel) {
    if (SPICE_IS_PORT_CHANNEL(channel)) {
        if (SPICE_PORT_CHANNEL(channel) == port.channel) {
            port.channel = NULL;
            g_object_unref(port.cancellable);
            port.cancellable = NULL;
#ifndef WIN32
        } else {
            serialChannelDestroy(SPICE_PORT_CHANNEL(channel));
#endif
        }
    }
}


void flexvdi_port_register_session(gpointer session) {
    g_signal_connect(session, "channel-new",
                     G_CALLBACK(channel_new), NULL);
    g_signal_connect(session, "channel-destroy",
                     G_CALLBACK(channel_destroy), NULL);
    //     g_signal_connect(session, "notify::migration-state",
    //                      G_CALLBACK(migration_state), port);
    initPrintClient();
}


int flexvdi_is_agent_connected(void) {
    return port.channel != NULL;
}


int flexvdi_send_credentials(const gchar *username, const gchar *password,
                             const gchar *domain) {
    if (port.channel) {
        flexvdiSpiceSendCredentials(username, password, domain);
        return TRUE;
    } else {
        flexvdiLog(L_WARN, "The flexVDI guest agent is not connected");
        return FALSE;
    }
}


int flexvdi_get_printer_list(GSList ** printer_list) {
    return flexvdiSpiceGetPrinterList(printer_list);
}


int flexvdi_share_printer(const char * printer) {
    if (port.channel) {
        return flexvdiSpiceSharePrinter(printer);
    } else {
        flexvdiLog(L_WARN, "The flexVDI guest agent is not connected");
        return FALSE;
    }
}


int flexvdi_unshare_printer(const char * printer) {
    if (port.channel) {
        return flexvdiSpiceUnsharePrinter(printer);
    } else {
        flexvdiLog(L_WARN, "The flexVDI guest agent is not connected");
        return FALSE;
    }
}


void flexvdiSpiceSendCredentials(const char * username, const char * password,
                                 const char * domain) {
    uint8_t * buf;
    size_t bufSize, i;
    FlexVDICredentialsMsg msgTemp, * msg;
    msgTemp.userLength = username ? strnlen(username, 1024) : 0;
    msgTemp.passLength = password ? strnlen(password, 1024) : 0;
    msgTemp.domainLength = domain ? strnlen(domain, 1024) : 0;
    bufSize = sizeof(msgTemp) + msgTemp.userLength +
                msgTemp.passLength + msgTemp.domainLength + 3;
    buf = getMsgBuffer(bufSize);
    if (!buf) {
        flexvdiLog(L_WARN, "Unable to reserve memory for credentials message");
        return;
    }
    msg = (FlexVDICredentialsMsg *)buf;
    i = 0;
    memcpy(msg, &msgTemp, sizeof(msgTemp));
    memcpy(msg->strings, username, msgTemp.userLength);
    msg->strings[i += msgTemp.userLength] = '\0';
    memcpy(&msg->strings[++i], password, msgTemp.passLength);
    msg->strings[i += msgTemp.passLength] = '\0';
    memcpy(&msg->strings[++i], domain, msgTemp.domainLength);
    msg->strings[i += msgTemp.domainLength] = '\0';
    sendMessage(FLEXVDI_CREDENTIALS, buf);
}
