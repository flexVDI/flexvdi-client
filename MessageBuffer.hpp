/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _MESSAGEBUFFER_HPP_
#define _MESSAGEBUFFER_HPP_

#include <memory>
#include <boost/asio.hpp>
#include "FlexVDIProto.h"

namespace flexvm {

class MessageBuffer;

template <typename MsgClass>
class MessagePtr {
public:
    friend class MessageBuffer;

    MsgClass * get() const noexcept { return p.get(); }
    MsgClass & operator*() const noexcept { return *get(); }
    MsgClass * operator->() const noexcept { return get(); }

private:
    MessagePtr(const std::shared_ptr<uint8_t> b) :
        p(b, (MsgClass *)(b.get() + sizeof(FlexVDIMessageHeader))) {}
    std::shared_ptr<MsgClass> p;
};


class MessageBuffer {
public:
    MessageBuffer() : buffer(nullptr, 0) {}

    // Build the buffer from a received header
    template <typename Deleter = std::default_delete<uint8_t[]>>
    MessageBuffer(const FlexVDIMessageHeader & h, Deleter d = Deleter())
    : data(new uint8_t[h.size + HEADER_SIZE], d), buffer(data.get(), h.size + HEADER_SIZE) {
        header() = h;
    }

    // Build the buffer from a message type and payload size
    template <typename Deleter = std::default_delete<uint8_t[]>>
    MessageBuffer(uint32_t type, std::size_t size, Deleter d = Deleter())
    : data(new uint8_t[size + HEADER_SIZE], d), buffer(data.get(), size + HEADER_SIZE) {
        header().size = size;
        header().type = type;
    }

    // Build the buffer from a message pointer (to another buffer)
    template <typename MsgClass>
    MessageBuffer(const MessagePtr<MsgClass> & msg)
    : data(msg.p, (uint8_t *)msg.get() - HEADER_SIZE),
      buffer(data.get(), header().size + HEADER_SIZE) {}

    bool isValid() const { return data.get() != nullptr; }

    FlexVDIMessageHeader & header() const {
        return *((FlexVDIMessageHeader *)data.get());
    }

    template <typename MsgClass>
    MessagePtr<MsgClass> getMessagePtr() const { return MessagePtr<MsgClass>(data); }

    uint8_t * getMsgData() const { return data.get() + HEADER_SIZE; }
    std::shared_ptr<uint8_t> shareData() const { return data; }
    std::size_t size() const { return header().size + HEADER_SIZE; }

    // Implement the ConstBufferSequence requirements.
    typedef boost::asio::const_buffer value_type;
    typedef const value_type * const_iterator;
    const_iterator begin() const { return &buffer; }
    const_iterator end() const { return &buffer + 1; }

private:
    static const std::size_t HEADER_SIZE = sizeof(FlexVDIMessageHeader);

    std::shared_ptr<uint8_t> data;
    boost::asio::const_buffers_1 buffer;
};

} // namespace flexvm

#endif // _MESSAGEBUFFER_HPP_
