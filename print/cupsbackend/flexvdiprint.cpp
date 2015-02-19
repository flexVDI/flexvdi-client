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
#include "../../util.hpp"
#include "../send-job.hpp"
using namespace std;
using namespace flexvm;
namespace asio = boost::asio;


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

    string jobId(argv[1]), jobCopies(argv[4]);
    string cupsUser(argv[2]), jobTitle(argv[3]), jobOptions(argv[5]);

    Log(L_DEBUG) << "Printing " << jobCopies << " copies of job " << jobId <<
    ": \"" << jobTitle << "\", from " << cupsUser << " with options: " << jobOptions;

    jobOptions += string(" copies=") + jobCopies;
    jobOptions += string(" title=\"") + jobTitle + "\"";
    ifstream inFile;
    if (argc > 6) inFile.open(argv[6]);
    bool result = sendJob(argc > 6 ? inFile : cin, jobOptions);
    Log(L_DEBUG) << "Finished sending the PDF file";

    return result ? 0 : 1;
}
