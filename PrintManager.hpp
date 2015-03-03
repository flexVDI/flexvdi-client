/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _PRINTMANAGER_HPP_
#define _PRINTMANAGER_HPP_

#include <memory>
#include <fstream>
#include <list>
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

private:
    struct Job {
        Connection::Ptr src;
        uint32_t id;

        Job(const Connection::Ptr & src, uint32_t id) : src(src), id(id) {}
        ~Job() { src->close(); }
    };

    std::list<Job> jobs;

    uint32_t getNewId() const {
        if (jobs.empty()) return 1;
        else return jobs.back().id + 1;
    }

    void closed(const Connection::Ptr & src, const boost::system::error_code & error);

    static bool installPrinter(const std::string & printer, const std::string & ppd);
    static bool uninstallPrinter(const std::string & printer);
};

}

#endif // _PRINTMANAGER_HPP_
