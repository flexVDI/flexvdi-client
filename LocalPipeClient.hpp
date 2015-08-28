/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _LOCALPIPECLIENT_HPP_
#define _LOCALPIPECLIENT_HPP_

#include <memory>
#include "Connection.hpp"

namespace flexvm {

#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
class LocalPipeClient : public StreamConnection<boost::asio::windows::stream_handle> {
#else
class LocalPipeClient : public StreamConnection<boost::asio::local::stream_protocol::socket> {
#endif
public:
    typedef std::shared_ptr<LocalPipeClient> Ptr;
    static constexpr const char * defaultPipeName = "flexvdi_pipe";

    static Ptr create(boost::asio::io_service & io, Connection::MessageHandler handler,
                      const std::string & endpointName) {
        Ptr result = Ptr(new LocalPipeClient(io, handler));
        return result->open(endpointName) ? result : Ptr();
    }
    static Ptr create(boost::asio::io_service & io, Connection::MessageHandler handler) {
        return create(io, handler, defaultPipeName);
    }

private:
    LocalPipeClient(boost::asio::io_service & io, Connection::MessageHandler handler) :
                    StreamConnection(io, handler) {}
    bool open(const std::string & endpointName);
};

}

#endif // _LOCALPIPECLIENT_HPP_
