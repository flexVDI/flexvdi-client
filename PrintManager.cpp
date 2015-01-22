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
        char * newline, * equalSign = next;
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
    string filename = pdfFilename(msg);
    if (filename.empty()) {
        Log(L_ERROR) << "No filename supplied for this print job";
    } else {
        uint32_t id = getNewId();
        jobs.emplace_back();
        Job & job = jobs.back();
        job.id = id;
        job.pdfFile.open(filename.c_str());
        if (job.pdfFile.bad()) {
            jobs.pop_back();
            return_if(true, "Could not open " << filename, );
        }
        msg->id = id;
        Connection::Ptr spiceClient = FlexVDIGuestAgent::singleton().spiceClient();
        spiceClient->send(FLEXVDI_PRINTJOB,
                          SharedConstBuffer(msg, getPrintJobMsgSize(msg.get())),
                          [this]() { sendNextBlock(jobs.back()); });
    }
}


void PrintManager::sendNextBlock(Job & job) {
    const std::size_t BLOCK_SIZE = 4096;

    if (job.pdfFile) {
        std::shared_ptr<char> block(new char[BLOCK_SIZE], std::default_delete<char[]>());
        job.pdfFile.read(block.get(), BLOCK_SIZE);
        uint32_t size = job.pdfFile.gcount();
        if (size) {
            std::shared_ptr<FlexVDIPrintJobDataMsg> msg(new FlexVDIPrintJobDataMsg);
            msg->id = job.id;
            msg->dataLength = size;
            Connection::Ptr spiceClient = FlexVDIGuestAgent::singleton().spiceClient();
            spiceClient->send(FLEXVDI_PRINTJOB,
                SharedConstBuffer(msg, getPrintJobDataMsgSize(msg.get()))(block, size),
                [this]() { sendNextBlock(jobs.back()); });
        }
    }
}
