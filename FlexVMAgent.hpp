/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <asio.hpp>
#include "VirtioPort.hpp"

#ifndef _FLEXVMAGENT_HPP_
#define _FLEXVMAGENT_HPP_

namespace flexvm {

class FlexVMAgent {
public:
    FlexVMAgent(int argc, char * argv[]) : port(io) {
        parseOptions(argc, argv);
    }

    void parseOptions(int argc, char * argv[]);
    int run();

private:
    asio::io_service io;
    VirtioPort port;
};

} // namespace flexvm

#endif // _FLEXVMAGENT_HPP_
