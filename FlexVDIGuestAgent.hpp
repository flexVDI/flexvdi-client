/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDIAGENT_HPP_
#define _FLEXVDIAGENT_HPP_

#include <memory>
#include <boost/asio.hpp>
#include "DispatcherRegistry.hpp"
#include "VirtioPort.hpp"
#include "CredentialsManager.hpp"

namespace flexvm {

class FlexVDIGuestAgent : public VirtioPort::Handler {
public:
    FlexVDIGuestAgent(int argc, char * argv[]) : port(io, *this), creds(io) {
        parseOptions(argc, argv);
    }

    void parseOptions(int argc, char * argv[]);
    int run();
    virtual void handle(uint32_t type, const std::shared_ptr<uint8_t> & msgBuffer);

private:
    boost::asio::io_service io;
    DispatcherRegistry dregistry;
    VirtioPort port;
    CredentialsManager creds;

    void registerHandlers();
};

} // namespace flexvm

#endif // _FLEXVDIAGENT_HPP_
