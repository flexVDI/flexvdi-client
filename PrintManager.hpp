/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _PRINTMANAGER_HPP_
#define _PRINTMANAGER_HPP_

#include <memory>
#include <fstream>
#include <list>
#include "FlexVDIGuestAgent.hpp"

namespace flexvm {

class PrintManager : public FlexVDIComponent<PrintManager
,FlexVDIPrintJobMsg
,FlexVDIPrintJobDataMsg
> {
public:
    typedef MessagePtr<FlexVDIPrintJobMsg> FlexVDIPrintJobMsgPtr;
    typedef MessagePtr<FlexVDIPrintJobDataMsg> FlexVDIPrintJobDataMsgPtr;

    void handle(const Connection::Ptr & src, const FlexVDIPrintJobMsgPtr & msg);
    void handle(const Connection::Ptr & src, const FlexVDIPrintJobDataMsgPtr & msg);

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
};

}

#endif // _PRINTMANAGER_HPP_
