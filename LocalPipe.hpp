/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _LOCALPIPE_HPP_
#define _LOCALPIPE_HPP_

#include <memory>
#include <list>
#include "Connection.hpp"

namespace flexvm {

class LocalPipe {
public:
    LocalPipe(boost::asio::io_service & io, Connection::MessageHandler handler);
    ~LocalPipe();

    void open();
    void setEndpoint(const std::string & name) {
        endpointName = name;
    }

private:
    static constexpr const char * defaultPipeName = "flexvdi_pipe";
    std::string endpointName;
    Connection::MessageHandler handler;
    class LocalConnection;
    std::list<std::shared_ptr<Connection>> connections;
    struct Listener;
    Listener * listener;

    void listen();
    void connectionAccepted(const boost::system::error_code & error);
};

}

#endif // _LOCALPIPE_HPP_
