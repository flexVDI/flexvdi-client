/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "FlexVDIProto.h"

typedef int (*MarshallingFunction)(uint8_t *, size_t bytes);

static MarshallingFunction marshallers[FLEXVDI_MAX_MESSAGE_TYPE];

static int marshallFlexVDICredentialsMsg(uint8_t * data, size_t bytes) {
    if (bytes < sizeof(FlexVDICredentialsMsg)) {
        return 0;
    }
    FlexVDICredentialsMsg * msg = (FlexVDICredentialsMsg *)data;
    BYTESWAP32(msg->userLength);
    BYTESWAP32(msg->passLength);
    BYTESWAP32(msg->domainLength);
    return bytes >= sizeof(FlexVDICredentialsMsg) +
        msg->userLength + msg->passLength + msg->domainLength + 3;
}

void registerMessageMarshallers() {
    marshallers[FLEXVDI_CREDENTIALS] = marshallFlexVDICredentialsMsg;
}

int marshallMessage(uint32_t type, uint8_t * data, size_t bytes) {
    if (type < FLEXVDI_MAX_MESSAGE_TYPE) {
        return marshallers[type](data, bytes);
    } else {
        return 0;
    }
}
