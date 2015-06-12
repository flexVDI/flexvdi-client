/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDIAGENT_HPP_
#define _FLEXVDIAGENT_HPP_

#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include "DispatcherRegistry.hpp"
#include "VirtioPort.hpp"
#include "LocalPipe.hpp"

namespace flexvm {

class FlexVDIGuestAgent {
public:
    FlexVDIGuestAgent() :
        port(io, dregistry.asMessageHandler()),
        pipe(io, dregistry.asMessageHandler()) {}

    int run();
    void stop();
    void setVirtioEndpoint(const std::string & name) {
        port.setEndpoint(name);
    }
    void setLocalEndpoint(const std::string & name) {
        pipe.setEndpoint(name);
    }
    DispatcherRegistry & getDispatcherRegistry() { return dregistry; }

private:
    boost::asio::io_service io;
    DispatcherRegistry dregistry;
    VirtioPort port;
    LocalPipe pipe;
};

} // namespace flexvm

#endif // _FLEXVDIAGENT_HPP_
