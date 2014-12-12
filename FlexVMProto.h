/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVMPROTO_H_
#define _FLEXVMPROTO_H_

#include <stdint.h>
#include <spice/macros.h>
#include "config.h"

#ifdef WORDS_BIGENDIAN
#define BYTESWAP32(val) (uint32_t)val = SPICE_BYTESWAP32((uint32_t)val)
#else
#define BYTESWAP32(val)
#endif

SPICE_BEGIN_DECLS

typedef struct FlexVMMessageHeader {
    uint32_t type;
    uint32_t size;
} FlexVMMessageHeader;

static inline void marshallHeader(FlexVMMessageHeader * header) {
    BYTESWAP32(header->type);
    BYTESWAP32(header->size);
}

void registerMessageMarshallers();
int marshallMessage(uint32_t type, uint8_t * data, size_t bytes);

enum {
    FLEXVM_CREDENTIALS = 0,
    FLEXVM_MAX_MESSAGE_TYPE // Must be the last one
};

typedef struct FlexVMCredentialsMsg {
    uint32_t userLength;
    uint32_t passLength;
    uint32_t domainLength;
    char strings[0];
} FlexVMCredentialsMsg;

static inline const char * getCredentialsUser(const FlexVMCredentialsMsg * msg) {
    return &msg->strings[0];
}
static inline const char * getCredentialsPassword(const FlexVMCredentialsMsg * msg) {
    return &msg->strings[msg->userLength + 1];
}
static inline const char * getCredentialsDomain(const FlexVMCredentialsMsg * msg) {
    return &msg->strings[msg->userLength + msg->passLength + 2];
}

SPICE_END_DECLS

#endif // _FLEXVMPROTO_H_
