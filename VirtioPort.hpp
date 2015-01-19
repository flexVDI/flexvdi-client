/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _VIRTIOPORT_HPP_
#define _VIRTIOPORT_HPP_

#include <memory>
#include "Connection.hpp"

namespace flexvm {

class VirtioPort {
public:
    VirtioPort(boost::asio::io_service & io, Connection::MessageHandler handler);

    void open();
    void setEndpoint(const std::string & name) {
        endpointName = name;
    }
    Connection::Ptr spiceClient();

private:
    static constexpr const char * defaultPortName = "es.flexvdi.guest_agent";
    std::string endpointName;
    class VirtioConnection;
    std::shared_ptr<VirtioConnection> conn;
};

} // namespace flexvm

#endif // _VIRTIOPORT_HPP_
