/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include "flexvdi-spice.h"
#include "flexvdi-port.h"
#include "spice-client.h"
#define FLEXVDI_PROTO_IMPL
#include "FlexVDIProto.h"
#include "printclient.h"


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
    g_logv("flexvdi", map[level], format, args);
    va_end(args);
}


static size_t freadsecure(gchar ** buffer, FILE * file) {
    size_t count = 0, bufferSize = 0, i;
    do {
        gchar * c = *buffer;
        *buffer = g_malloc(bufferSize += 1024);
        memcpy(*buffer, c, count);
        for (i = 0; i < count; ++i)
            c[i] = '\0';
        c = *buffer + count;
        for (; count < bufferSize; ++c, ++count) {
            int r = fgetc(file);
            if (r == '\n' || r == EOF) break;
            *c = r & 255;
        }
    } while (count == bufferSize);
    (*buffer)[count] = '\0';
    return count;
}


static gchar * username, * password, * domain;
static gboolean parseUsernameOpt(const gchar * optionName, const gchar * value,
                                 gpointer data, GError ** error) {
    username = g_strdup(value);
    size_t bytesRead = freadsecure(&password, stdin);
    if (bytesRead > 0) {
        return TRUE;
    } else {
        g_warning("Failed to read password from stdin");
        return FALSE;
    }
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


static void port_write_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    if (!g_cancellable_is_cancelled(port.cancellable)) {
        spice_port_write_finish(port.channel, res, &error);
        if (error != NULL)
            g_warning("%s", error->message);
        g_clear_error(&error);
    }
    g_free(user_data);
}


void sendMessage(uint32_t type, uint8_t * buffer) {
    FlexVDIMessageHeader * head = (FlexVDIMessageHeader *)(buffer - HEADER_SIZE);
    size_t size = head->size + HEADER_SIZE;
    head->type = type;
    marshallMessage(type, buffer, head->size);
    marshallHeader(head);
    spice_port_write_async(port.channel, head, size, port.cancellable, port_write_cb, head);
}


static void port_opened(SpiceChannel *channel, GParamSpec *pspec) {
    gchar *name = NULL;
    gboolean opened = FALSE;
    g_object_get(channel, "port-name", &name, "port-opened", &opened, NULL);
    if (g_strcmp0(name, "es.flexvdi.guest_agent") == 0 && opened) {
        flexvdiLog(L_INFO, "flexVDI guest agent connected");
        port.channel = SPICE_PORT_CHANNEL(channel);
        port.cancellable = g_cancellable_new();
        uint8_t * buf = getMsgBuffer(0);
        if (buf) {
            sendMessage(FLEXVDI_RESET, buf);
        }
        if (username && password) {
            flexvdi_send_credentials(username, password, domain ? domain : "");
        }
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
    if (pchannel != port.channel) return;

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
                    g_warning("Wrong message size on reception");
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


static const GOptionEntry cmdline_entries[] = {
    // { long_name, short_name, flags, arg, arg_data, description, arg_description },
    { "flexvdi-user", 0, 0, G_OPTION_ARG_CALLBACK, parseUsernameOpt,
      N_("User name for Single Sign-on. Provide password through stdin."), N_("<username>") },
    { "flexvdi-domain", 0, 0, G_OPTION_ARG_STRING, &domain,
      N_("Domain name for Single Sign-on."), N_("<domainname>") },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};


GOptionGroup * flexvdi_get_option_group(void) {
    username = password = domain = NULL;
    GOptionGroup * grp = g_option_group_new("flexvdi", _("flexVDI Options:"),
                                            _("Show flexVDI Options"), NULL, NULL);
    g_option_group_add_entries(grp, cmdline_entries);

    return grp;
}


void flexvdi_send_credentials(const gchar *username, const gchar *password,
                              const gchar *domain) {
    if (port.channel) {
        flexvdiSpiceSendCredentials(username, password, domain);
    }
}


void flexvdi_get_printer_list(GSList ** printer_list) {
    flexvdiSpiceGetPrinterList(printer_list);
}


void flexvdi_share_printer(const char * printer) {
    if (port.channel) {
        flexvdiSpiceSharePrinter(printer);
    }
}


void flexvdi_unshare_printer(const char * printer) {
    if (port.channel) {
        flexvdiSpiceUnsharePrinter(printer);
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
        g_warning("Unable to reserve memory for credentials message");
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
