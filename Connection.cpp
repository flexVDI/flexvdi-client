/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "Connection.hpp"
#include "util.hpp"

using namespace flexvm;
namespace asio = boost::asio;
namespace sys = boost::system;
using namespace std::placeholders;


std::map<std::string, Connection::Ptr> Connection::namedConnections;


void Connection::readNextMessage() {
    asyncRead(asio::buffer(&header, sizeof(FlexVDIMessageHeader)),
              std::bind(&Connection::readCompleteHeader, this, shared_from_this(), _1, _2));
}


void Connection::readNextByte() {
    size_t i = 0;
    uint8_t * buf = (uint8_t *)&header;
    while (i < sizeof(FlexVDIMessageHeader) - 1) {
        buf[i] = buf[i + 1];
        ++i;
    }
    asyncRead(asio::buffer(&buf[i], 1),
              std::bind(&Connection::readCompleteHeader, this, shared_from_this(), _1, _2));
}


void Connection::readError(const boost::system::error_code & error) {
    if (error == asio::error::eof)
        Log(L_INFO) << "Connection " << this << " reset by peer.";
    else
        Log(L_WARNING) << "Error reading message from connection " << this << ": "
                        << error.message();
    notifyError(error);
}


void Connection::readCompleteHeader(Ptr This, const sys::error_code & error,
                                    std::size_t bytes) {
    if (error) {
        readError(error);
    } else {
        marshallHeader(&header);
        Log(L_DEBUG) << "Reading message type " << header.type <<
                        " and size " << header.size << " from connection " << this;
        if (header.size > FLEXVDI_MAX_MESSAGE_LENGTH) {
            Log(L_WARNING) << "Oversized message (" << header.size
                           << " > " << FLEXVDI_MAX_MESSAGE_LENGTH << ')';
            readNextByte();
        } else if (header.type >= FLEXVDI_MAX_MESSAGE_TYPE) {
            Log(L_WARNING) << "Unknown message type " << header.type;
            readNextByte();
        } else {
            readBuffer = MessageBuffer(header);
            asyncRead(asio::buffer(readBuffer.getMsgData(), header.size),
                      std::bind(&Connection::readCompleteBody, this,
                                shared_from_this(), _1, _2));
        }
    }
}


void Connection::readCompleteBody(Ptr This, const sys::error_code & error,
                                  std::size_t bytes) {
    if (error) {
        readError(error);
    } else {
        // TODO Is bytes_transferred == header.size ??
        if (!marshallMessage(header.type, readBuffer.getMsgData(), bytes)) {
            Log(L_WARNING) << "Message size mismatch on connection " << this;
        } else {
            mHandler(shared_from_this(), readBuffer);
        }
        readNextMessage();
    }
}


void Connection::send(const MessageBuffer & msgBuffer,
                      std::function<void(void)> onSuccess) {
    auto & header = msgBuffer.header();
    Log(L_DEBUG) << "Writing message type " << header.type <<
                 " and size " << header.size << " to connection " << this;
    marshallMessage(header.type, msgBuffer.getMsgData(), header.size);
    marshallHeader(&(msgBuffer.header()));
    asyncWrite(msgBuffer, std::bind(&Connection::writeComplete, this,
                                    shared_from_this(), _1, _2, onSuccess));
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
