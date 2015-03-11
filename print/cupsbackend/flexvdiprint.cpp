/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <boost/asio.hpp>
#include <cups/cups.h>
#include "../../util.hpp"
#include "../send-job.hpp"
using namespace std;
using namespace flexvm;
namespace asio = boost::asio;


string getPrinterName(int jobId) {
    string printer;
    cups_job_t * jobs;
    int numJobs = cupsGetJobs(&jobs, nullptr, 0, CUPS_WHICHJOBS_ACTIVE);
    for (int i = 0; i < numJobs && printer.empty(); ++i) {
        if (jobs[i].id == jobId) {
            printer = jobs[i].dest;
        }
    }
    cupsFreeJobs(numJobs, jobs);
    return printer;
}


class CupsConnection {
public:
    CupsConnection(const string & printer) {
        string name = printer;
        size_t slashPos = name.find_first_of('/');
        numDests = cupsGetDests(&dests);
        if (dests) {
            if (slashPos != string::npos)
                dest = cupsGetDest(name.substr(0, slashPos).c_str(),
                                   name.substr(slashPos + 1).c_str(), numDests, dests);
            else dest = cupsGetDest(name.c_str(), NULL, numDests, dests);
            if (dest) {
                http = cupsConnectDest(dest, CUPS_DEST_FLAGS_NONE,
                                       30000, NULL, NULL, 0, NULL, NULL);
                if (http) {
                    dinfo = cupsCopyDestInfo(http, dest);
                }
            }
        }
    }
    ~CupsConnection() {
        cupsFreeDestInfo(dinfo);
        httpClose(http);
        cupsFreeDests(numDests, dests);
    }

    bool isOpen() const { return dinfo != NULL; }

    string getDefault(const string & option, const string & fallback) {
        ipp_attribute_t * attr = cupsFindDestDefault(http, dest, dinfo, option.c_str());
        return attr ? string(ippGetString(attr, 0, NULL)) : fallback;
    }
    string getSharedPrinterName() {
        const char * name = cupsGetOption("flexvdi-shared-printer",
                                          dest->num_options, dest->options);
        return name ? name : "";
    }

private:
    cups_dest_t * dests, * dest;
    int numDests;
    cups_dinfo_t * dinfo;
    http_t * http;
};


string parseOptions(char * argv[6]) {
    string jobCopies(argv[4]);
    int jobId = stoi(argv[1]);
    string printer = getPrinterName(jobId), defaultVal;
    string cupsUser(argv[2]), jobTitle(argv[3]), jobOptions(argv[5]);

    CupsConnection conn(printer);
    if (!conn.isOpen()) {
        Log(L_ERROR) << "Could not open printer " << printer << ": " << cupsLastErrorString();
        return string();
    }

    size_t pos;
    jobOptions += string(" printer=\"") + conn.getSharedPrinterName() + "\"";
    jobOptions += string(" title=\"") + jobTitle + "\"";
    jobOptions += string(" copies=") + jobCopies;
    if (jobOptions.find("noCollate") == string::npos
        && jobOptions.find("Collate") == string::npos) {
        jobOptions += " noCollate";
    }
    if (jobOptions.find("media=") == string::npos) {
        jobOptions += " media=" + conn.getDefault("media", "a4");
    }
    if (jobOptions.find("sides=") == string::npos) {
        jobOptions += " sides=" + conn.getDefault("sides", "one-sided");
    }
    if (jobOptions.find("Resolution=") == string::npos) {
        jobOptions += " Resolution=" + conn.getDefault("Resolution", "300dpi");
    }
    if ((pos = jobOptions.find("ColorModel=")) != string::npos
        && jobOptions.length() > pos + 11 && jobOptions[pos + 11] == 'G') {
        jobOptions += " gray";
    } else {
        jobOptions += " color";
    }

    Log(L_DEBUG) << "Printing with options: " << jobOptions;
    return jobOptions;
}


int main(int argc, char * argv[]) {
    if (argc == 1) {
        cout << "direct flexvdiprint \"Unknown\" \"flexVDI Printer\"" << endl;
        return 0;
    }

    Log::setLogOstream(&cerr);
    Log::setPrefix<>([](Log & log, LogLevel level) {
        static const char * levelStr[] = {
            "DEBUG",
            "INFO",
            "WARN",
            "ERROR"
        };
        log << levelStr[level] << ": ";
    });

    string jobOptions = parseOptions(argv);
    if (!jobOptions.empty()) {
        ifstream inFile;
        if (argc > 6) inFile.open(argv[6]);
        bool result = sendJob(argc > 6 ? inFile : cin, jobOptions);
        Log(L_DEBUG) << "Finished sending the PDF file";

        return result ? 0 : 1;
    } else return 1;
}
