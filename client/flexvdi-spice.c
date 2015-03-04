/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "flexvdi-spice.h"
#define FLEXVDI_PROTO_IMPL
#include "FlexVDIProto.h"
#include "printclient.h"


static FlexVDISpiceCallbacks * cb;

void flexvdiSpiceInit(FlexVDISpiceCallbacks * callbacks) {
    cb = callbacks;
    initPrintClient();
}


void flexvdiLog(FlexVDILogLevel level, const char * format, ...) {
    va_list args;
    va_start(args, format);
    cb->log(level, format, args);
    va_end(args);
}

static const size_t HEADER_SIZE = sizeof(FlexVDIMessageHeader);

static uint8_t * getMsgBuffer(size_t size) {
    uint8_t * buf = (uint8_t *)g_malloc(size + HEADER_SIZE);
    if (buf) {
        ((FlexVDIMessageHeader *)buf)->size = size;
        return buf + HEADER_SIZE;
    } else
        return NULL;
}


static void sendMessage(uint32_t type, uint8_t * buffer) {
    FlexVDIMessageHeader * head = (FlexVDIMessageHeader *)(buffer - HEADER_SIZE);
    size_t size = head->size + HEADER_SIZE;
    head->type = type;
    marshallMessage(type, buffer, head->size);
    marshallHeader(head);
    cb->sendMessage(head, size);
}


void flexvdiSpiceConnected() {
    flexvdiLog(L_INFO, "flexVDI guest agent connected\n");
    uint8_t * buf = getMsgBuffer(0);
    if (buf) {
        sendMessage(FLEXVDI_RESET, buf);
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


int flexvdiSpiceData(void * data, size_t size) {
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
                        return 1;
                    }
                    handleMessage(curHeader.type, buffer);
                    free(buffer);
                    state = WAIT_NEW_MESSAGE;
                }
                break;
        }
    }

    return 0;
}


int flexvdiSpiceSendCredentials(const char * username, const char * password,
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
        return 1;
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
    return 0;
}


int flexvdiSpiceSharePrinter(const char * printer) {
    size_t bufSize, nameLength, ppdLength;
    GStatBuf statbuf;
    char * ppdName = getPPDFile(printer);
    int result = 0;
    nameLength = strnlen(printer, 1024);
    if (!g_stat(ppdName, &statbuf)) {
        ppdLength = statbuf.st_size;
        bufSize = sizeof(FlexVDISharePrinterMsg) + nameLength + 1 + ppdLength;
        uint8_t * buf = getMsgBuffer(bufSize);
        if (buf) {
            FILE * ppd = g_fopen(ppdName, "r");
            if (ppd) {
                FlexVDISharePrinterMsg * msg = (FlexVDISharePrinterMsg *)buf;
                msg->printerNameLength = nameLength;
                msg->ppdLength = ppdLength;
                strncpy(msg->data, printer, nameLength + 1);
                fread(&msg->data[nameLength + 1], 1, ppdLength, ppd);
                sendMessage(FLEXVDI_SHAREPRINTER, buf);
            } else result = 1;
            fclose(ppd);
        } else result = 1;
    } else result = 1;
    g_unlink(ppdName);
    g_free(ppdName);
    return result;
}


int flexvdiSpiceUnsharePrinter(const char * printer) {
    size_t nameLength = strnlen(printer, 1024);
    size_t bufSize = sizeof(FlexVDIUnsharePrinterMsg) + nameLength + 1;
    uint8_t * buf = getMsgBuffer(bufSize);
    if (buf) {
        FlexVDIUnsharePrinterMsg * msg = (FlexVDIUnsharePrinterMsg *)buf;
        msg->printerNameLength = nameLength;
        strncpy(msg->printerName, printer, nameLength + 1);
        sendMessage(FLEXVDI_UNSHAREPRINTER, buf);
    } else return 1;
    return 0;
}
