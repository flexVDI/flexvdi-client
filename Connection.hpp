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
#include "MessageBuffer.hpp"
#include "util.hpp"

namespace flexvm {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    typedef std::shared_ptr<Connection> Ptr;
    typedef std::function<void(const Ptr &, const MessageBuffer &)> MessageHandler;
    typedef std::function<void(const Ptr &, const boost::system::error_code &)> ErrorHandler;

    Connection(MessageHandler h) : mHandler(h) {}
    virtual ~Connection() {}

    void send(const MessageBuffer & msgBuffer, std::function<void(void)> onSuccess = [](){});
    void registerErrorHandler(ErrorHandler e) {
        eHandlers.push_back(e);
    }
    virtual bool isOpen() const = 0;
    virtual void close() = 0;

protected:
    MessageHandler mHandler;
    std::list<ErrorHandler> eHandlers;

    void readNextMessage();
    void readNextByte();
    void notifyError(const boost::system::error_code & error) {
        // Prevent this from being destroyed before the end of the method
        Ptr This = shared_from_this();
        for (auto & handler : eHandlers)
            handler(This, error);
    }

    typedef std::function<void(const boost::system::error_code &, std::size_t)> AsyncHandler;
    virtual void asyncRead(const boost::asio::mutable_buffers_1 & buffers,
                           AsyncHandler handler) = 0;
    virtual void asyncWrite(const MessageBuffer & buffer, AsyncHandler handler) = 0;

private:
    MessageBuffer readBuffer;
    FlexVDIMessageHeader header;

    void readCompleteHeader(Ptr This, const boost::system::error_code & error,
                            std::size_t bytes);
    void readCompleteBody(Ptr This, const boost::system::error_code & error,
                          std::size_t bytes);
    void readError(const boost::system::error_code & error);
    void writeComplete(Ptr This, const boost::system::error_code & error,
                       std::size_t bytes, std::function<void(void)> onSuccess);
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
    virtual void close() {
        boost::system::error_code ec;
        stream.close(ec);
        stream = stream_t(stream.get_io_service());
    }

protected:
    stream_t stream;

private:
    virtual void asyncRead(const boost::asio::mutable_buffers_1 & buffers,
                           AsyncHandler handler) {
        boost::asio::async_read(stream, buffers, handler);
    }
    virtual void asyncWrite(const MessageBuffer & buffer, AsyncHandler handler) {
        boost::asio::async_write(stream, buffer, handler);
    }
};


} // namespace flexvm

#endif // _CONNECTION_HPP_
