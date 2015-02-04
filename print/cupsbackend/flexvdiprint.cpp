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
#include "../../FlexVDIProto.h"
using namespace std;
using namespace flexvm;
namespace asio = boost::asio;


static int notifyJob(const string fileName) {
    asio::io_service io;
    // TODO: Configurable
    asio::local::stream_protocol::endpoint ep("/tmp/flexvdi_pipe");
    asio::local::stream_protocol::socket pipe(io);
    boost::system::error_code error;
    return_if(pipe.connect(ep, error), "Failed opening pipe", 1);
    size_t optionsLength = fileName.length() + 9; // 9 = length of "filename="
    size_t bufSize = sizeof(FlexVDIMessageHeader) + sizeof(FlexVDIPrintJobMsg) + optionsLength;
    unique_ptr<uint8_t[]> buffer(new uint8_t[bufSize]);
    FlexVDIMessageHeader * header = (FlexVDIMessageHeader *)buffer.get();
    header->type = FLEXVDI_PRINTJOB;
    header->size = bufSize - sizeof(FlexVDIMessageHeader);
    FlexVDIPrintJobMsg * msg = (FlexVDIPrintJobMsg *)(header + 1);
    msg->optionsLength = optionsLength;
    copy_n("filename=", 9, msg->options);
    copy_n(fileName.c_str(), fileName.length(), &msg->options[9]);
    return_if(asio::write(pipe, asio::buffer(buffer.get(), bufSize)) < bufSize,
              "Error notifying print job", 1);
    // Wait until connection is closed, to avoid races deleting the file
    asio::read(pipe, asio::buffer(buffer.get(), bufSize));
    return 0;
}


static int printToFlexVDI(char * argv[], istream & inFile) {
    int jobId = stoi(argv[1]), jobCopies = stoi(argv[4]);
    string cupsUser(argv[2]), jobTitle(argv[3]), jobOptions(argv[5]);

    Log(L_DEBUG) << "Printing " << jobCopies << " copies of job " << jobId <<
    ": \"" << jobTitle << "\", from " << cupsUser << " with options: " << jobOptions;

    char outFileName[] = "/tmp/fpjXXXXXX.pdf";
    int outFd = mkstemps(outFileName, 4);
    return_if(outFd == -1, "Failed to create output file", -1);

    while (inFile) {
        char buffer[1024];
        inFile.read(buffer, 1024);
        write(outFd, buffer, inFile.gcount());
    }

    close(outFd);
    int result = notifyJob(outFileName);
    unlink(outFileName);

    return result;
}


int main(int argc, char * argv[]) {
    if (argc == 1) {
        cout << "direct flexvdiprint \"Unknown\" \"flexVDI Printer\"" << endl;
        return 0;
    } else {
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

        ifstream inFile;
        if (argc >= 7) inFile.open(argv[6]);
        return printToFlexVDI(argv, argc < 7 ? cin : inFile);
    }
}
