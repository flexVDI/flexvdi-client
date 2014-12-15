/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <memory>
#include <algorithm>
#include <asio.hpp>
#include "../FlexVDIProto.h"
#include "../SharedConstBuffer.hpp"
using namespace std;

int main(int argc, char * argv[]) {
    if (argc != 2) {
        clog << "Use: send_credentials pipe" << endl;
        return 1;
    }
    registerMessageMarshallers();
    auto mHeader = new FlexVDIMessageHeader;
    mHeader->type = FLEXVDI_CREDENTIALS;
    mHeader->size = sizeof(FlexVDICredentialsMsg) + 15;
    auto msg = new FlexVDICredentialsMsg;
    msg->domainLength = msg->passLength = msg->userLength = 4;
    auto msgBuffer = new uint8_t[15];
    std::copy_n("user", 5, msgBuffer);
    std::copy_n("pass", 5, msgBuffer + 5);
    std::copy_n("domi", 5, msgBuffer + 10);
    asio::io_service io;
    asio::posix::stream_descriptor stream(io, ::open(argv[1], O_RDWR));
    asio::write<>(stream, flexvm::SharedConstBuffer(mHeader)(msg)(msgBuffer, 15));
    return 0;
}
