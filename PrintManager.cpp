/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <map>
#include <fstream>
#include "PrintManager.hpp"
#include "util.hpp"

using namespace flexvm;
using std::string;


REGISTER_COMPONENT(PrintManager);


static string pdfFilename(const PrintManager::FlexVDIPrintJobMsgPtr & msg) {
    char * next = msg->options, * end = msg->options + msg->optionsLength;
    while (next < end) {
        char * newline = next, * equalSign = next;
        while (newline < end && *newline != '\n') ++newline;
        while (equalSign < newline && *equalSign != '=') ++equalSign;
        string key(next, equalSign);
        if (key == "filename") {
            if (equalSign < newline)
                return string(equalSign + 1, newline);
            else
                return string("");
        }
        next = newline + 1;
    }
    return string("");
}


void PrintManager::handle(const Connection::Ptr & src, const FlexVDIPrintJobMsgPtr & msg) {
    Log(L_DEBUG) << "Ready to send a new print job";
    string filename = pdfFilename(msg);
    if (filename.empty()) {
        Log(L_ERROR) << "No filename supplied for this print job";
    } else {
        Log(L_DEBUG) << "Reading file " << filename;
        jobs.emplace_back(filename, getNewId());
        Job & job = jobs.back();
        if (!job.pdfFile) {
            jobs.pop_back();
            Log(L_ERROR) << "Could not open " << filename << lastSystemError("").what();
            return;
        }
        job.src = src;
        msg->id = job.id;
        msg->dataLength = job.getFileLength();
        Connection::Ptr spiceClient = FlexVDIGuestAgent::singleton().spiceClient();
        spiceClient->send(FLEXVDI_PRINTJOB,
                          SharedConstBuffer(msg, getPrintJobMsgSize(msg.get())),
                          [this, &job]() { sendNextBlock(job); });
    }
}


void PrintManager::sendNextBlock(Job & job) {
    const std::size_t BLOCK_SIZE = 4096;

    if (job.pdfFile) {
        std::shared_ptr<char> block(new char[BLOCK_SIZE], std::default_delete<char[]>());
        job.pdfFile.read(block.get(), BLOCK_SIZE);
        uint32_t size = job.pdfFile.gcount();
        std::shared_ptr<FlexVDIPrintJobDataMsg> msg(new FlexVDIPrintJobDataMsg);
        msg->id = job.id;
        msg->dataLength = size;
        uint32_t msgSize = sizeof(FlexVDIPrintJobDataMsg);
        Connection::Ptr spiceClient = FlexVDIGuestAgent::singleton().spiceClient();
        spiceClient->send(FLEXVDI_PRINTJOBDATA,
                            SharedConstBuffer(msg, msgSize)(block, size),
                            [this, &job, size]() { sendNextBlock(job); });
    } else {
        Log(L_DEBUG) << "Finished sending the PDF file, remove it";
        jobs.remove_if([&job](const Job & j) { return job.id == j.id; });
    }
}
