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
> {
public:
    typedef std::shared_ptr<FlexVDIPrintJobMsg> FlexVDIPrintJobMsgPtr;

    void handle(const Connection::Ptr & src, const FlexVDIPrintJobMsgPtr & msg);

private:
    struct Job {
        std::ifstream pdfFile;
        uint32_t id;
    };

    std::list<Job> jobs;

    void sendNextBlock(Job & job);
    uint32_t getNewId() const {
        if (jobs.empty()) return 1;
        else return jobs.back().id + 1;
    }
};

}

#endif // _PRINTMANAGER_HPP_
