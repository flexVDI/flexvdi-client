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

static inline void marshallHeader(FlexVDIMessageHeader * header) {
    BYTESWAP32(header->type);
    BYTESWAP32(header->size);
}

int marshallMessage(uint32_t type, uint8_t * data, size_t bytes);

enum {
    FLEXVDI_CREDENTIALS = 0,
    FLEXVDI_ASKCREDENTIALS,
    FLEXVDI_PRINTJOB,
    FLEXVDI_MAX_MESSAGE_TYPE // Must be the last one
};


typedef struct FlexVDICredentialsMsg {
    uint32_t userLength;
    uint32_t passLength;
    uint32_t domainLength;
    char strings[0];
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
static inline uint32_t getCredentialsMsgSize(const FlexVDICredentialsMsg * msg) {
    return sizeof(FlexVDICredentialsMsg) +
        msg->userLength + msg->passLength + msg->domainLength + 3;
}


typedef struct FlexVDIEmptyMsg {
} FlexVDIAskCredentialsMsg;


typedef struct FlexVDIPrintJobMsg {
    uint32_t optionsLength;
    uint32_t dataLength;
    char options[0];
} FlexVDIPrintJobMsg;

static inline const uint8_t * getPrintJobData(const FlexVDIPrintJobMsg * msg) {
    return (const uint8_t *)&msg->options[msg->optionsLength];
}
static inline uint32_t getPrintJobMsgSize(const FlexVDIPrintJobMsg * msg) {
    return sizeof(FlexVDIPrintJobMsg) + msg->optionsLength + msg->dataLength;
}


#ifdef FLEXVDI_PROTO_IMPL
typedef int (*MarshallingFunction)(uint8_t *, size_t bytes);

static int marshallEmptyMsg(uint8_t * data, size_t bytes) {
    return TRUE;
}

static int marshallFlexVDICredentialsMsg(uint8_t * data, size_t bytes) {
    if (bytes < sizeof(FlexVDICredentialsMsg)) {
        return 0;
    }
    FlexVDICredentialsMsg * msg = (FlexVDICredentialsMsg *)data;
    BYTESWAP32(msg->userLength);
    BYTESWAP32(msg->passLength);
    BYTESWAP32(msg->domainLength);
    return bytes >= getCredentialsMsgSize(msg);
}

static int marshallFlexVDIPrintJobMsg(uint8_t * data, size_t bytes) {
    if (bytes < sizeof(FlexVDIPrintJobMsg)) {
        return 0;
    }
    FlexVDIPrintJobMsg * msg = (FlexVDIPrintJobMsg *)data;
    BYTESWAP32(msg->optionsLength);
    BYTESWAP32(msg->dataLength);
    return bytes >= getPrintJobMsgSize(msg);
}

static MarshallingFunction marshallers[FLEXVDI_MAX_MESSAGE_TYPE] = {
    [FLEXVDI_CREDENTIALS] = marshallFlexVDICredentialsMsg,
    [FLEXVDI_ASKCREDENTIALS] = marshallEmptyMsg,
    [FLEXVDI_PRINTJOB] = marshallFlexVDIPrintJobMsg,
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
