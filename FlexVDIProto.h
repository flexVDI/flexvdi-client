/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDIPROTO_H_
#define _FLEXVDIPROTO_H_

#include <stdint.h>
#include <spice/macros.h>
#include "config.h"

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
    FLEXVDI_CREDENTIALS = 0,
    FLEXVDI_ASKCREDENTIALS,
    FLEXVDI_PRINTJOB,
    FLEXVDI_PRINTJOBDATA,
    FLEXVDI_SHAREPRINTER,
    FLEXVDI_UNSHAREPRINTER,
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


typedef struct FlexVDIEmptyMsg {
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

#ifdef FLEXVDI_PROTO_IMPL

typedef int (*MarshallingFunction)(uint8_t *, size_t);
typedef size_t (*SizeFunction)(const uint8_t *);

#define PROTO_SIZE_FUNC(Msg, extra) \
static size_t get ## Msg ## Size(const uint8_t * data) { \
    const Msg * msg = (const Msg *)data; \
    return sizeof(*msg) + extra; \
}

PROTO_SIZE_FUNC(FlexVDICredentialsMsg, msg->userLength + msg->passLength + msg->domainLength + 3)
PROTO_SIZE_FUNC(FlexVDIAskCredentialsMsg, 0)
PROTO_SIZE_FUNC(FlexVDIPrintJobMsg, msg->optionsLength)
PROTO_SIZE_FUNC(FlexVDIPrintJobDataMsg, msg->dataLength)
PROTO_SIZE_FUNC(FlexVDISharePrinterMsg, msg->printerNameLength + msg->ppdLength + 1)
PROTO_SIZE_FUNC(FlexVDIUnsharePrinterMsg, msg->printerNameLength + 1)

static SizeFunction sizeFunctions[FLEXVDI_MAX_MESSAGE_TYPE] = {
    [FLEXVDI_CREDENTIALS] = getFlexVDICredentialsMsgSize,
    [FLEXVDI_ASKCREDENTIALS] = getFlexVDIAskCredentialsMsgSize,
    [FLEXVDI_PRINTJOB] = getFlexVDIPrintJobMsgSize,
    [FLEXVDI_PRINTJOBDATA] = getFlexVDIPrintJobDataMsgSize,
    [FLEXVDI_SHAREPRINTER] = getFlexVDISharePrinterMsgSize,
    [FLEXVDI_UNSHAREPRINTER] = getFlexVDIUnsharePrinterMsgSize,
};

size_t messageSize(uint32_t type, const uint8_t * data) {
    if (type < FLEXVDI_MAX_MESSAGE_TYPE) {
        return sizeFunctions[type](data);
    } else {
        return 0;
    }
}


#define PROTO_MARSHALL_FUNC(Msg, commands) \
static int marshall ## Msg(uint8_t * data, size_t bytes) { \
    if (bytes < sizeof(Msg)) return 0; \
    Msg * msg = (Msg *)data; \
    commands \
    return bytes >= get ## Msg ## Size((const uint8_t *)msg); \
}

PROTO_MARSHALL_FUNC(FlexVDICredentialsMsg,
                    BYTESWAP32(msg->userLength);
                    BYTESWAP32(msg->passLength);
                    BYTESWAP32(msg->domainLength);
)
PROTO_MARSHALL_FUNC(FlexVDIPrintJobMsg,
                    BYTESWAP32(msg->id);
                    BYTESWAP32(msg->optionsLength);
)
PROTO_MARSHALL_FUNC(FlexVDIPrintJobDataMsg,
                    BYTESWAP32(msg->id);
                    BYTESWAP32(msg->dataLength);
)
PROTO_MARSHALL_FUNC(FlexVDISharePrinterMsg,
                    BYTESWAP32(msg->printerNameLength);
                    BYTESWAP32(msg->ppdLength);
)
PROTO_MARSHALL_FUNC(FlexVDIUnsharePrinterMsg,
                    BYTESWAP32(msg->printerNameLength);
)

static int marshallEmptyMsg(uint8_t * data, size_t bytes) {
    return TRUE;
}

static MarshallingFunction marshallers[FLEXVDI_MAX_MESSAGE_TYPE] = {
    [FLEXVDI_CREDENTIALS] = marshallFlexVDICredentialsMsg,
    [FLEXVDI_ASKCREDENTIALS] = marshallEmptyMsg,
    [FLEXVDI_PRINTJOB] = marshallFlexVDIPrintJobMsg,
    [FLEXVDI_PRINTJOBDATA] = marshallFlexVDIPrintJobDataMsg,
    [FLEXVDI_SHAREPRINTER] = marshallFlexVDISharePrinterMsg,
    [FLEXVDI_UNSHAREPRINTER] = marshallFlexVDIUnsharePrinterMsg,
};

int marshallMessage(uint32_t type, uint8_t * data, size_t bytes) {
    if (type < FLEXVDI_MAX_MESSAGE_TYPE) {
        return marshallers[type](data, bytes);
    } else {
        return 0;
    }
}
#endif

SPICE_END_DECLS

#endif // _FLEXVDIPROTO_H_
