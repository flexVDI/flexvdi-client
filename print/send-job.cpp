/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#include "send-job.hpp"
#include "util.hpp"
#include "FlexVDIProto.h"
#include "LocalPipeClient.hpp"

using namespace std;
using namespace flexvm;
namespace asio = boost::asio;
namespace sys = boost::system;


static MessageBuffer getPrintJobMsg(const string & options) {
    std::size_t size = sizeof(FlexVDIPrintJobMsg) + options.length();
    MessageBuffer result(FLEXVDI_PRINTJOB, size);
    auto msg = result.getMessagePtr<FlexVDIPrintJobMsg>();
    msg->optionsLength = options.length();
    copy_n(options.c_str(), msg->optionsLength, msg->options);
    Log(L_DEBUG) << "Sending job with options: " << options;
    return result;
}


static MessageBuffer getPrintJobData(istream & pdfFile) {
    const size_t BLOCK_SIZE = 4096;
    std::size_t size = sizeof(FlexVDIPrintJobDataMsg) + BLOCK_SIZE;
    MessageBuffer result(FLEXVDI_PRINTJOBDATA, size);
    auto msg = result.getMessagePtr<FlexVDIPrintJobDataMsg>();
    pdfFile.read(msg->data, BLOCK_SIZE);
    msg->dataLength = pdfFile.gcount();
    Log(L_DEBUG) << "Sending block of " << msg->dataLength << " bytes";
    result.updateSize(sizeof(FlexVDIPrintJobDataMsg) + msg->dataLength);
    return result;
}


bool flexvm::sendJob(istream & pdfFile, const string & options) {
    asio::io_service io;
    bool result = false;
    Connection::Ptr client =
        LocalPipeClient::create(io, [](const Connection::Ptr &, const MessageBuffer &){});
    if (!client.get()) return false;
    client->registerErrorHandler([&](const Connection::Ptr &, const sys::error_code & error) {
        Log(L_ERROR) << "Error sending print job data";
        io.stop();
    });
    std::function<void(void)> sendNextBlock = [&]() {
        if (pdfFile) client->send(getPrintJobData(pdfFile), sendNextBlock);
        else {
            result = true;
            io.stop();
        }
    };
    client->send(getPrintJobMsg(options), sendNextBlock);
    io.run();
    client->close();
    return result;
}
