/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#include <unistd.h>
#ifdef WIN32
#include <windows.h>
#include <winspool.h>
#else
#include <cups/cups.h>
#endif
#include "PrintManager.hpp"
#include "util.hpp"

using namespace flexvm;
using std::string;
using std::wstring;
namespace ph = std::placeholders;


REGISTER_COMPONENT(PrintManager);


PrintManager::PrintManager() {
    thread = std::thread(std::bind(&PrintManager::workingThread, this));
}


void PrintManager::workingThread() {
    bool stop = false;
    while (!stop) {
        Task task = dequeueTask();
        switch (task.type) {
        case Task::STOPTHREAD:
            stop = true;
            break;
        case Task::SHARE:
            Log(L_DEBUG) << "Installing printer " << task.printer
                         << (installPrinter(task.printer, task.ppd) ? " succeeded" : " failed");
            unlink(task.ppd.c_str());
            break;
        case Task::UNSHARE:
            Log(L_DEBUG) << "Uninstalling printer " << task.printer
                         << (uninstallPrinter(task.printer) ? " succeeded" : " failed");
            break;
        case Task::RESET:
            resetPrinters();
            break;
        }
    }
}


void PrintManager::stopWorkingThread() {
    if (thread.joinable()) {
        enqueueTask(Task{Task::STOPTHREAD, "", ""});
        thread.join();
    }
}


void PrintManager::enqueueTask(const Task & task) {
    {
        std::unique_lock<std::mutex> lock(tasksMutex);
        tasks.push_front(task);
    }
    hasTasks.notify_one();
}


PrintManager::Task PrintManager::dequeueTask() {
    bool skip = false;
    Task task;
    do {
        std::unique_lock<std::mutex> lock(tasksMutex);
        while (tasks.empty()) {
            hasTasks.wait(lock);
        }
        task = tasks.back();
        tasks.pop_back();
        auto skipType = task.type == Task::SHARE ? Task::UNSHARE : Task::SHARE;
        for (auto it = tasks.begin(); it != tasks.end() && !skip; ++it)
            skip = it->type == skipType && it->printer == task.printer;
    } while (skip);
    return task;
}


void PrintManager::handle(const Connection::Ptr & src, const PrintJobMsgPtr & msg) {
    Log(L_DEBUG) << "Ready to send a new print job";
    jobs.emplace_back(src, getNewId());
    Job & job = jobs.back();
    msg->id = job.id;
    src->registerErrorHandler(std::bind(&PrintManager::closed, this, ph::_1, ph::_2));
    Connection::Ptr client = Connection::getNamedConnection("spice-client");
    if (client)
        client->send(MessageBuffer(msg));
}


void PrintManager::handle(const Connection::Ptr & src, const PrintJobDataMsgPtr & msg) {
    auto job = std::find_if(jobs.begin(), jobs.end(),
                            [&src](const Job & j) { return src == j.src; });
    if (job == jobs.end()) {
        Log(L_WARNING) << "Received print job data for an unknown job";
        return;
    }
    msg->id = job->id;
    Connection::Ptr client = Connection::getNamedConnection("spice-client");
    if (client)
        client->send(MessageBuffer(msg));
}


void PrintManager::handle(const Connection::Ptr & src, const SharePrinterMsgPtr & msg) {
    string printer(getPrinterName(msg.get()));
    const char * ppdText = getPPD(msg.get()), * ppdEnd = ppdText + msg->ppdLength;
    const char * key = "*PCFileName: \"";
    const int keyLength = 14;
    string fileName;
    for (const char * pos = ppdText; pos < ppdEnd - keyLength; ++pos) {
        if (!strncmp(pos, key, keyLength)) {
            pos += keyLength;
            const char * end = strchr(pos, '"');
            if (end) {
                fileName = getTempDirName() + string(pos, end);
                break;
            }
        }
    }
    if (fileName.empty()) fileName = getTempFileName("fv") + ".ppd";
    std::ofstream ppdFile(fileName.c_str());
    ppdFile.write(ppdText, msg->ppdLength);
    enqueueTask(Task{Task::SHARE, printer, fileName});
}


void PrintManager::handle(const Connection::Ptr & src, const UnsharePrinterMsgPtr & msg) {
    string printer(msg->printerName, msg->printerNameLength);
    enqueueTask(Task{Task::UNSHARE, printer, ""});
}


void PrintManager::handle(const Connection::Ptr & src, const ResetMsgPtr & msg) {
    enqueueTask(Task{Task::RESET, "", ""});
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
    MessageBuffer buffer(FLEXVDI_PRINTJOBDATA, sizeof(FlexVDIPrintJobDataMsg));
    auto msg = (FlexVDIPrintJobDataMsg *)buffer.getMsgData();
    msg->id = job->id;
    msg->dataLength = 0;
    Connection::Ptr client = Connection::getNamedConnection("spice-client");
    if (client)
        client->send(buffer);
    jobs.erase(job);
}


const char * PrintManager::sharedPrinterDescription = "flexVDI Shared Printer";
const char * PrintManager::sharedPrinterLocation = "flexVDI Client";
