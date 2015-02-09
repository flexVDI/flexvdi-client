/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#include <boost/asio.hpp>
#include "send-job.hpp"
#include "../util.hpp"
#include "../FlexVDIProto.h"
#include "../SharedConstBuffer.hpp"

#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
#include <windows.h>
#endif

using namespace std;
using namespace flexvm;
namespace asio = boost::asio;


static SharedConstBuffer getPrintJobMsg() {
    auto * header = new FlexVDIMessageHeader;
    header->type = FLEXVDI_PRINTJOB;
    header->size = sizeof(FlexVDIPrintJobMsg);
    auto * msg = new FlexVDIPrintJobMsg;
    // TODO: add job options
    msg->optionsLength = 0;
    return SharedConstBuffer(header)(msg);
}


static SharedConstBuffer getPrintJobData(istream & pdfFile) {
    const std::size_t BLOCK_SIZE = 4096;
    std::shared_ptr<char> block(new char[BLOCK_SIZE], std::default_delete<char[]>());
    pdfFile.read(block.get(), BLOCK_SIZE);
    uint32_t size = pdfFile.gcount();
    auto * header = new FlexVDIMessageHeader;
    header->type = FLEXVDI_PRINTJOBDATA;
    header->size = sizeof(FlexVDIPrintJobDataMsg) + size;
    auto * msg = new FlexVDIPrintJobDataMsg;
    msg->dataLength = size;
    return SharedConstBuffer(header)(msg)(block, size);
}


template <class Pipe>
static bool sendData(std::istream & pdfFile, Pipe & pipe) {
    SharedConstBuffer jobMsgBuffer = getPrintJobMsg();
    return_if(asio::write(pipe, jobMsgBuffer) < jobMsgBuffer.size(),
              "Error notifying print job", false);
    while (pdfFile) {
        SharedConstBuffer jobDataBuffer = getPrintJobData(pdfFile);
        return_if(asio::write(pipe, jobDataBuffer) < jobDataBuffer.size(),
                  "Error sending print job data", false);
    }

    return true;
}


bool flexvm::sendJob(std::istream & pdfFile) {
    asio::io_service io;
    boost::system::error_code error;
#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
    HANDLE h;
    h = ::CreateFileA("\\\\.\\pipe\\flexvdi_pipe", GENERIC_READ | GENERIC_WRITE,
                      0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    return_if(h == INVALID_HANDLE_VALUE, "Failed opening pipe", false);
    asio::windows::stream_handle pipe(io, h);
#else
    asio::local::stream_protocol::endpoint ep("/var/run/flexvdi_pipe");
    asio::local::stream_protocol::socket pipe(io);
    return_if(pipe.connect(ep, error), "Failed opening pipe", false);
#endif

    bool result = sendData(pdfFile, pipe);
    pipe.close();
    return result;
}
