/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#include <boost/asio.hpp>
#include "send-job.hpp"
#include "util.hpp"
#include "FlexVDIProto.h"
#include "MessageBuffer.hpp"

#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
#include <windows.h>
#endif

using namespace std;
using namespace flexvm;
namespace asio = boost::asio;


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


template <class Pipe>
static bool sendData(istream & pdfFile, Pipe & pipe, const string & options) {
    MessageBuffer jobMsgBuffer = getPrintJobMsg(options);
    return_if(asio::write(pipe, jobMsgBuffer) < jobMsgBuffer.size(),
              "Error notifying print job", false);
    while (pdfFile) {
        MessageBuffer jobDataBuffer = getPrintJobData(pdfFile);
        return_if(asio::write(pipe, jobDataBuffer) < jobDataBuffer.size(),
                  "Error sending print job data", false);
    }
    return true;
}


bool flexvm::sendJob(istream & pdfFile, const string & options) {
    asio::io_service io;
    boost::system::error_code error;
#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
    HANDLE h;
    h = ::CreateFile(L"\\\\.\\pipe\\flexvdi_pipe", GENERIC_READ | GENERIC_WRITE,
                     0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    return_if(h == INVALID_HANDLE_VALUE, "Failed opening pipe", false);
    asio::windows::stream_handle pipe(io, h);
#else
    asio::local::stream_protocol::endpoint ep("/var/run/flexvdi_pipe");
    asio::local::stream_protocol::socket pipe(io);
    return_if(pipe.connect(ep, error), "Failed opening pipe", false);
#endif

    bool result = sendData(pdfFile, pipe, options);
    pipe.close();
    return result;
}
