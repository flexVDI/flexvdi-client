/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "Connection.hpp"
#include "util.hpp"

using namespace flexvm;
namespace asio = boost::asio;
namespace sys = boost::system;
using namespace std::placeholders;


void Connection::readComplete(Ptr This, const sys::error_code & error, std::size_t bytes) {
    if (error) {
        if (error == asio::error::eof)
            Log(L_INFO) << "Connection " << this << " reset by peer.";
        else
            Log(L_WARNING) << "Error reading message from connection " << this << ": "
                << error.message();
        notifyError(error);
    } else {
        if (!readBuffer.get()) {
            // It read the header
            marshallHeader(&header);
            Log(L_DEBUG) << "Reading message type " << header.type <<
                " and size " << header.size << " from connection " << this;
            readBuffer.reset(new uint8_t[header.size], std::default_delete<uint8_t[]>());
            asyncRead(asio::buffer(readBuffer.get(), header.size),
                      std::bind(&Connection::readComplete, this, shared_from_this(), _1, _2));
        } else {
            // It read the body
            // TODO Is bytes_transferred == header.size ??
            if (!marshallMessage(header.type, readBuffer.get(), bytes)) {
                Log(L_WARNING) << "Message size mismatch on connection " << this;
            } else {
                mHandler(shared_from_this(), header.type, readBuffer);
            }
            readNextMessage();
        }

    }
}


void Connection::writeComplete(Ptr This, const boost::system::error_code & error,
                               std::size_t bytes, std::function<void(void)> onSuccess) {
    if (error) {
        if (error == asio::error::eof)
            Log(L_INFO) << "Connection " << this << " reset by peer.";
        else
            Log(L_WARNING) << "Error writing message to connection " << this << ": "
                << error.message();
        notifyError(error);
    } else {
        onSuccess();
    }
}


void Connection::readNextMessage() {
    readBuffer.reset();
    asyncRead(asio::buffer(&header, sizeof(FlexVDIMessageHeader)),
              std::bind(&Connection::readComplete, this, shared_from_this(), _1, _2));
}


void Connection::send(uint32_t type, const SharedConstBuffer & msgBuffer,
                      std::function<void(void)> onSuccess) {
    uint32_t size = msgBuffer.size();
    auto mHeader = new FlexVDIMessageHeader{type, size};
    marshallHeader(mHeader);
    uint8_t * msgPtr = asio::buffer_cast<uint8_t *>(*msgBuffer.begin());
    marshallMessage(type, msgPtr, size);
    Log(L_DEBUG) << "Writing message type " << type <<
    " and size " << size << " to connection " << this;
    asyncWrite(SharedConstBuffer(mHeader)(msgBuffer),
               std::bind(&Connection::writeComplete, this,
                         shared_from_this(), _1, _2, onSuccess));
}
