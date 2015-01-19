/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "Connection.hpp"
#include "util.hpp"

using namespace flexvm;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace ph = std::placeholders;


void Connection::readComplete(const sys::error_code & error,
                              std::size_t bytes_transferred) {
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
                AsyncHandler(std::bind(&Connection::readComplete, this, ph::_1, ph::_2)));
        } else {
            // It read the body
            // TODO Is bytes_transferred == header.size ??
            if (!marshallMessage(header.type, readBuffer.get(), bytes_transferred)) {
                Log(L_WARNING) << "Message size mismatch on connection " << this;
            } else {
                mHandler(shared_from_this(), header.type, readBuffer);
            }
            readNextMessage();
        }

    }
}


void Connection::writeComplete(const boost::system::error_code & error,
                               std::size_t bytes_transferred) {
    if (error) {
        if (error == asio::error::eof)
            Log(L_INFO) << "Connection " << this << " reset by peer.";
        else
            Log(L_WARNING) << "Error writing message to connection " << this << ": "
                << error.message();
        notifyError(error);
    }
}


void Connection::readNextMessage() {
    readBuffer.reset();
    asyncRead(asio::buffer(&header, sizeof(FlexVDIMessageHeader)),
              std::bind(&Connection::readComplete, this, ph::_1, ph::_2));
}


void Connection::send(uint32_t type, uint32_t size,
                      const std::shared_ptr<uint8_t> & msgBuffer) {
    auto mHeader = new FlexVDIMessageHeader{type, size};
    asyncWrite(SharedConstBuffer(mHeader)(msgBuffer, size),
               std::bind(&Connection::writeComplete, this, ph::_1, ph::_2));
}
