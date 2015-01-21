/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _CONNECTION_HPP_
#define _CONNECTION_HPP_

#include <memory>
#include <functional>
#include <list>
#include <boost/asio.hpp>
#include "FlexVDIProto.h"
#include "SharedConstBuffer.hpp"
#include "util.hpp"

namespace flexvm {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    typedef std::shared_ptr<Connection> Ptr;
    typedef std::function<void(const Ptr &, uint32_t,
                               const std::shared_ptr<uint8_t> &)> MessageHandler;
    typedef std::function<void(const Ptr &, const boost::system::error_code &)> ErrorHandler;

    Connection(MessageHandler h) : mHandler(h) {}
    virtual ~Connection() {}

    void send(uint32_t type, uint32_t size, const std::shared_ptr<uint8_t> & msgBuffer);
    void registerErrorHandler(ErrorHandler e) {
        eHandlers.push_back(e);
    }
    virtual bool isOpen() const = 0;
    virtual void close() = 0;

protected:
    MessageHandler mHandler;
    std::list<ErrorHandler> eHandlers;

    void readNextMessage();
    void notifyError(const boost::system::error_code & error) {
        // Prevent this from being destroyed before the end of the method
        Ptr This = shared_from_this();
        for (auto & handler : eHandlers)
            handler(This, error);
    }

    typedef std::function<void(const boost::system::error_code &, std::size_t)> AsyncHandler;
    virtual void asyncRead(const boost::asio::mutable_buffers_1 & buffers,
                           AsyncHandler handler) = 0;
    virtual void asyncWrite(const SharedConstBuffer & buffers, AsyncHandler handler) = 0;

private:
    std::shared_ptr<uint8_t> readBuffer;
    FlexVDIMessageHeader header;

    void readComplete(Ptr This, const boost::system::error_code & error,
                      std::size_t bytes_transferred);
    void writeComplete(Ptr This, const boost::system::error_code & error,
                       std::size_t bytes_transferred);
};


template <class stream_t>
class StreamConnection : public Connection {
public:
    StreamConnection(boost::asio::io_service & io, MessageHandler h)
    : Connection(h), stream(io) {}
    virtual ~StreamConnection() {
        stream.close();
    }
    virtual bool isOpen() const { return stream.is_open(); }
    virtual void close() {boost::system::error_code ec; stream.close(ec); }

protected:
    stream_t stream;

private:
    virtual void asyncRead(const boost::asio::mutable_buffers_1 & buffers,
                           AsyncHandler handler) {
        boost::asio::async_read(stream, buffers, handler);
    }
    virtual void asyncWrite(const SharedConstBuffer & buffers, AsyncHandler handler) {
        boost:: asio::async_write(stream, buffers, handler);
    }
};

} // namespace flexvm

#endif // _CONNECTION_HPP_
