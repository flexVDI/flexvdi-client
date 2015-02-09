/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <map>
#include <fstream>
#include "PrintManager.hpp"
#include "util.hpp"

using namespace flexvm;
using std::string;
namespace ph = std::placeholders;


REGISTER_COMPONENT(PrintManager);


void PrintManager::handle(const Connection::Ptr & src, const FlexVDIPrintJobMsgPtr & msg) {
    Log(L_DEBUG) << "Ready to send a new print job";
    jobs.emplace_back(src, getNewId());
    Job & job = jobs.back();
    msg->id = job.id;
    src->registerErrorHandler(std::bind(&PrintManager::closed, this, ph::_1, ph::_2));
    Connection::Ptr client = FlexVDIGuestAgent::singleton().spiceClient();
    client->send(FLEXVDI_PRINTJOB, SharedConstBuffer(msg, getPrintJobMsgSize(msg.get())));
}


void PrintManager::handle(const Connection::Ptr & src,
                          const FlexVDIPrintJobDataMsgPtr & msg) {
    auto job = std::find_if(jobs.begin(), jobs.end(),
                            [&src](const Job & j) { return src == j.src; });
    if (job == jobs.end()) {
        Log(L_WARNING) << "Received print job data for an unknown job";
        return;
    }
    msg->id = job->id;
    Connection::Ptr client = FlexVDIGuestAgent::singleton().spiceClient();
    client->send(FLEXVDI_PRINTJOBDATA,
                 SharedConstBuffer(msg, getPrintJobDataMsgSize(msg.get())));
}


void PrintManager::closed(const Connection::Ptr & src,
                          const boost::system::error_code & error) {
    auto job = std::find_if(jobs.begin(), jobs.end(),
                            [&src](const Job & j) { return src == j.src; });
    if (job == jobs.end()) {
        Log(L_WARNING) << "No print job associated with this connection";
        return;
    }
    Log(L_DEBUG) << "Finished sending the PDF file, remove the job";
    // Send a last block of 0 bytes to signal the end of the job
    auto * msg = new FlexVDIPrintJobDataMsg{job->id, 0};
    Connection::Ptr client = FlexVDIGuestAgent::singleton().spiceClient();
    client->send(FLEXVDI_PRINTJOBDATA, SharedConstBuffer(msg));
    jobs.erase(job);
}
