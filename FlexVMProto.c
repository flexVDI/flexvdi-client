/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "FlexVMProto.h"

typedef int (*MarshallingFunction)(uint8_t *, size_t bytes);

static MarshallingFunction marshallers[FLEXVM_MAX_MESSAGE_TYPE];

static int marshallFlexVMCredentialsMsg(uint8_t * data, size_t bytes) {
    if (bytes < sizeof(FlexVMCredentialsMsg)) {
        return 0;
    }
    FlexVMCredentialsMsg * msg = (FlexVMCredentialsMsg *)data;
    BYTESWAP32(msg->userLength);
    BYTESWAP32(msg->passLength);
    BYTESWAP32(msg->domainLength);
    return bytes >= sizeof(FlexVMCredentialsMsg) +
        msg->userLength + msg->passLength + msg->domainLength + 3;
}

void registerMessageMarshallers() {
    marshallers[FLEXVM_CREDENTIALS] = marshallFlexVMCredentialsMsg;
}

int marshallMessage(uint32_t type, uint8_t * data, size_t bytes) {
    if (type < FLEXVM_MAX_MESSAGE_TYPE) {
        return marshallers[type](data, bytes);
    } else {
        return 0;
    }
}
