/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDIPROTO_H_
#define _FLEXVDIPROTO_H_

#include <stdint.h>
#include <spice/macros.h>
#include "config.h"

/**
 * How to add new message types and not break old clients:
 * - ALWAYS add new message type constants at the end of the list, just
 *   above FLEXVDI_MAX_MESSAGE_TYPE. Otherwise other constant values
 *   may change. Even better, assign them a specific value. Constants
 *   MUST be consecutive, some functions expect them that way.
 * - NEVER change a message structure that has been commited to master branch.
 *   Add a new message if more information is needed or to deprecate an old one.
 **/

#ifdef WORDS_BIGENDIAN
#define BYTESWAP32(val) (uint32_t)val = SPICE_BYTESWAP32((uint32_t)val)
#else
#define BYTESWAP32(val)
#endif

SPICE_BEGIN_DECLS

typedef struct FlexVDIMessageHeader {
    uint32_t type;
    uint32_t size;
} FlexVDIMessageHeader;

#define FLEXVDI_MAX_MESSAGE_LENGTH 65536

static inline void marshallHeader(FlexVDIMessageHeader * header) {
    BYTESWAP32(header->type);
    BYTESWAP32(header->size);
}

int marshallMessage(uint32_t type, uint8_t * data, size_t bytes);
size_t messageSize(uint32_t type, const uint8_t * data);

enum {
    FLEXVDI_CREDENTIALS             = 0,
    FLEXVDI_ASKCREDENTIALS          = 1,
    FLEXVDI_PRINTJOB                = 2,
    FLEXVDI_PRINTJOBDATA            = 3,
    FLEXVDI_SHAREPRINTER            = 4,
    FLEXVDI_UNSHAREPRINTER          = 5,
    FLEXVDI_RESET                   = 6,
    FLEXVDI_MAX_MESSAGE_TYPE // Must be the last one
};


typedef struct FlexVDICredentialsMsg {
    uint32_t userLength;
    uint32_t passLength;
    uint32_t domainLength;
    char strings[0]; // user + '\0' + password + '\0' + domain + '\0'
} FlexVDICredentialsMsg;

static inline const char * getCredentialsUser(const FlexVDICredentialsMsg * msg) {
    return &msg->strings[0];
}
static inline const char * getCredentialsPassword(const FlexVDICredentialsMsg * msg) {
    return &msg->strings[msg->userLength + 1];
}
static inline const char * getCredentialsDomain(const FlexVDICredentialsMsg * msg) {
    return &msg->strings[msg->userLength + msg->passLength + 2];
}


typedef struct FlexVDIAskCredentialsMsg {
} FlexVDIAskCredentialsMsg;


typedef struct FlexVDIPrintJobMsg {
    uint32_t id;
    uint32_t optionsLength;
    char options[0];
} FlexVDIPrintJobMsg;


typedef struct FlexVDIPrintJobDataMsg {
    uint32_t id;
    uint32_t dataLength;
    char data[0];
} FlexVDIPrintJobDataMsg;


typedef struct FlexVDISharePrinterMsg {
    uint32_t printerNameLength;
    uint32_t ppdLength;
    char data[0]; // printerName + '\0' + PPD
} FlexVDISharePrinterMsg;

static inline const char * getPrinterName(const FlexVDISharePrinterMsg * msg) {
    return &msg->data[0];
}
static inline const char * getPPD(const FlexVDISharePrinterMsg * msg) {
    return &msg->data[msg->printerNameLength + 1];
}


typedef struct FlexVDIUnsharePrinterMsg {
    uint32_t printerNameLength;
    char printerName[0]; // printerName + '\0'
} FlexVDIUnsharePrinterMsg;


typedef struct FlexVDIResetMsg {
} FlexVDIResetMsg;


#ifdef FLEXVDI_PROTO_IMPL

#define MSG_OPERATIONS(Msg, id, extra, commands) \
case id: do { \
    Msg * msg = (Msg *)data; \
    if (op) return sizeof(*msg) + extra; \
    else { \
        if (bytes < sizeof(*msg)) return 0; \
        size_t size = sizeof(*msg) + extra; \
        commands \
        return bytes >= size; \
    } \
} while (0)

#define MSG_OPERATIONS_EMPTY(Msg, id) \
case id: return op ? sizeof(Msg) : TRUE

size_t msgOp(uint32_t type, int op, uint8_t * data, size_t bytes) {
    switch (type) {
        MSG_OPERATIONS(FlexVDICredentialsMsg, FLEXVDI_CREDENTIALS,
                       msg->userLength + msg->passLength + msg->domainLength + 3,
                       BYTESWAP32(msg->userLength);
                       BYTESWAP32(msg->passLength);
                       BYTESWAP32(msg->domainLength);
        );
        MSG_OPERATIONS_EMPTY(FlexVDIAskCredentialsMsg, FLEXVDI_ASKCREDENTIALS);
        MSG_OPERATIONS(FlexVDIPrintJobMsg, FLEXVDI_PRINTJOB,
                       msg->optionsLength,
                       BYTESWAP32(msg->id);
                       BYTESWAP32(msg->optionsLength);
        );
        MSG_OPERATIONS(FlexVDIPrintJobDataMsg, FLEXVDI_PRINTJOBDATA,
                       msg->dataLength,
                       BYTESWAP32(msg->id);
                       BYTESWAP32(msg->dataLength);
        );
        MSG_OPERATIONS(FlexVDISharePrinterMsg, FLEXVDI_SHAREPRINTER,
                       msg->printerNameLength + msg->ppdLength + 1,
                       BYTESWAP32(msg->printerNameLength);
                       BYTESWAP32(msg->ppdLength);
        );
        MSG_OPERATIONS(FlexVDIUnsharePrinterMsg, FLEXVDI_UNSHAREPRINTER,
                       msg->printerNameLength + 1,
                       BYTESWAP32(msg->printerNameLength);
        );
        MSG_OPERATIONS_EMPTY(FlexVDIResetMsg, FLEXVDI_RESET);
        default: return 0;
    }
}


size_t messageSize(uint32_t type, const uint8_t * data) {
    return msgOp(type, TRUE, (uint8_t *)data, 0);
}


int marshallMessage(uint32_t type, uint8_t * data, size_t bytes) {
    return msgOp(type, FALSE, data, bytes);
}
#endif

SPICE_END_DECLS

#endif // _FLEXVDIPROTO_H_
