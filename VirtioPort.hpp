/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _VIRTIOPORT_HPP_
#define _VIRTIOPORT_HPP_

#include <asio.hpp>

namespace flexvm {

class VirtioPort {
public:
    VirtioPort(asio::io_service & io);
    ~VirtioPort();

    void open();
    void setEndpoint(const std::string & name) {
        endpointName = name;
    }

private:
    std::string endpointName;

    struct VirtioPortImpl;
    VirtioPortImpl * impl;
};

} // namespace flexvm

#endif // _VIRTIOPORT_HPP_
