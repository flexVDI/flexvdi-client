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
    CupsConnection(const string & printer, const char * jobOptions) {
        options = NULL;
        numOptions = cupsParseOptions(jobOptions, 0, &options);
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
    string getDefaultResolution(const string & fallback) {
        ipp_attribute_t * attr = cupsFindDestDefault(http, dest, dinfo, "printer-resolution");
        ipp_res_t units;
        int yres, xres = attr ? ippGetResolution(attr, 0, &yres, &units) : 0;
        return attr ? (to_string(xres) + "dpi") : fallback;
    }
    string getSharedPrinterName() {
        const char * name = cupsGetOption("flexvdi-shared-printer",
                                          dest->num_options, dest->options);
        return name ? name : "";
    }
    string getOption(const char * name) {
        const char * option = cupsGetOption(name, numOptions, options);
        return option ? option : "";
    }
    int getOptionIndex(const char * name) {
        string option = getOption(name);
        int result = 0, nameLen = strlen(name);
        const char * ppdPath = cupsGetPPD(dest->name);
        if (ppdPath) {
            ifstream ppdFile(ppdPath);
            string line;
            int optionNumber = 0;
            while (ppdFile) {
                getline(ppdFile, line);
                if (line.empty()) continue;
                if (option.empty()) {
                    if (line.substr(1, nameLen + 7) == string("Default") + name) {
                        option = line.substr(nameLen + 10);
                        Log(L_DEBUG) << "Default value for " << name << " is " << option;
                    }
                } else {
                    if (line.substr(1, nameLen) == name) {
                        if (line.substr(nameLen + 2, option.length()) == option) {
                            result = optionNumber;
                            break;
                        }
                        ++optionNumber;
                    }
                }
            }
            unlink(ppdPath);
        }
        return result;
    }
    bool isGrayscale() const {
        ipp_attribute_t * attr = cupsFindDestSupported(http, dest, dinfo,
                                                       CUPS_PRINT_COLOR_MODE);
        return attr &&
                ippGetCount(attr) == 1 &&
                !strcmp(ippGetString(attr, 0, NULL), "monochrome");
    }

private:
    cups_dest_t * dests, * dest;
    int numDests;
    cups_dinfo_t * dinfo;
    http_t * http;
    cups_option_t * options;
    int numOptions;
};


string parseOptions(char * argv[6]) {
    string jobCopies(argv[4]);
    int jobId = stoi(argv[1]);
    string printer = getPrinterName(jobId), defaultVal;
    string cupsUser(argv[2]), jobTitle(argv[3]), jobOptions(argv[5]);

    CupsConnection conn(printer, argv[5]);
    if (!conn.isOpen()) {
        Log(L_ERROR) << "Could not open printer " << printer << ": " << cupsLastErrorString();
        return string();
    }

    jobOptions += string(" printer=\"") + conn.getSharedPrinterName() + "\"";
    jobOptions += string(" title=\"") + jobTitle + "\"";
    jobOptions += string(" copies=") + jobCopies;
    if (conn.getOption("Collate").empty()) {
        jobOptions += " noCollate";
    }
    if (!conn.getOption("PageSize").empty()) {
        jobOptions += " media=" + conn.getOption("PageSize");
    } else if (conn.getOption("media").empty()) {
        jobOptions += " media=" + conn.getDefault("media", "a4");
    }
    if (!conn.getOption("Duplex").empty()) {
        string duplex = conn.getOption("Duplex");
        if (duplex == "DuplexTumble") jobOptions += " sides=two-sided-short-edge";
        else if (duplex == "DuplexNoTumble") jobOptions += " sides=two-sided-long-edge";
        else  jobOptions += " sides=one-sided";
    } else if (conn.getOption("sides").empty()) {
        jobOptions += " sides=" + conn.getDefault("sides", "one-sided");
    }
    if (conn.getOption("Resolution").empty()) {
        jobOptions += " Resolution=" + conn.getDefaultResolution("300dpi");
    }
    if (conn.isGrayscale() || conn.getOption("ColorModel") == string("Gray")) {
        jobOptions += " gray";
    } else {
        jobOptions += " color";
    }
    jobOptions += " media-type=" + to_string(conn.getOptionIndex("MediaType"));
    jobOptions += " media-source=" + to_string(conn.getOptionIndex("InputSlot"));

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
