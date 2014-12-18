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
    FlexVDIGuestAgent() : port(io, *this), creds(io) {}

    int run();
    void stop();
    virtual void handle(uint32_t type, const std::shared_ptr<uint8_t> & msgBuffer);
    void setVirtioEndpoint(const std::string & name) {
        port.setEndpoint(name);
    }

private:
    boost::asio::io_service io;
    DispatcherRegistry dregistry;
    VirtioPort port;
    CredentialsManager creds;

    void registerHandlers();
};

} // namespace flexvm

#endif // _FLEXVDIAGENT_HPP_
