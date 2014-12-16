/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _VIRTIOPORT_HPP_
#define _VIRTIOPORT_HPP_

#include <boost/asio.hpp>
#include <memory>

namespace flexvm {

class VirtioPort {
public:
    class Handler {
    public:
        virtual void handle(uint32_t type, const std::shared_ptr<uint8_t> & msgBuffer) = 0;
    };

    VirtioPort(boost::asio::io_service & io, Handler & handler);
    ~VirtioPort();

    void open();
    void setEndpoint(const std::string & name) {
        endpointName = name;
    }

private:
    std::string endpointName;

    class Impl;
    Impl * impl;
};

} // namespace flexvm

#endif // _VIRTIOPORT_HPP_
