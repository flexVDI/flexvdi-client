/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _PRINTMANAGER_HPP_
#define _PRINTMANAGER_HPP_

#include <memory>
#include <fstream>
#include <list>
#include <boost/thread.hpp>
#include "DispatcherRegistry.hpp"

namespace flexvm {

class PrintManager : public FlexVDIComponent<PrintManager
,FlexVDIPrintJobMsg
,FlexVDIPrintJobDataMsg
,FlexVDISharePrinterMsg
,FlexVDIUnsharePrinterMsg
,FlexVDIResetMsg
> {
public:
    PrintManager();
    ~PrintManager() { stopWorkingThread(); }

    typedef MessagePtr<FlexVDIPrintJobMsg> PrintJobMsgPtr;
    typedef MessagePtr<FlexVDIPrintJobDataMsg> PrintJobDataMsgPtr;
    typedef MessagePtr<FlexVDISharePrinterMsg> SharePrinterMsgPtr;
    typedef MessagePtr<FlexVDIUnsharePrinterMsg> UnsharePrinterMsgPtr;
    typedef MessagePtr<FlexVDIResetMsg> ResetMsgPtr;

    void handle(const Connection::Ptr & src, const PrintJobMsgPtr & msg);
    void handle(const Connection::Ptr & src, const PrintJobDataMsgPtr & msg);
    void handle(const Connection::Ptr & src, const SharePrinterMsgPtr & msg);
    void handle(const Connection::Ptr & src, const UnsharePrinterMsgPtr & msg);
    void handle(const Connection::Ptr & src, const ResetMsgPtr & msg);

    static bool installPrinter(const std::string & printer, const std::string & ppd);
    static bool uninstallPrinter(const std::string & printer);
    static void resetPrinters();

    static const char * sharedPrinterDescription;
    static const char * sharedPrinterLocation;

private:
    struct Job {
        Connection::Ptr src;
        uint32_t id;

        Job(const Connection::Ptr & src, uint32_t id) : src(src), id(id) {}
        ~Job() { src->close(); }
    };

    struct Task {
        enum {
            STOPTHREAD,
            SHARE,
            UNSHARE,
            RESET,
        } type;
        std::string printer;
        std::string ppd;
    };

    boost::thread thread;
    std::list<Job> jobs;
    std::list<Task> tasks;
    boost::mutex tasksMutex;
    boost::condition_variable hasTasks;

    uint32_t getNewId() const {
        if (jobs.empty()) return 1;
        else return jobs.back().id + 1;
    }

    void closed(const Connection::Ptr & src, const boost::system::error_code & error);

    void enqueueTask(const Task & task);
    Task dequeueTask();
    void workingThread();
    void stopWorkingThread();
};

}

#endif // _PRINTMANAGER_HPP_
