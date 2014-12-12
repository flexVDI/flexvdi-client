/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _SHAREDCONSTBUFFER_HPP_
#define _SHAREDCONSTBUFFER_HPP_

#include <memory>
#include <list>
#include <asio.hpp>

namespace flexvm {

class SharedConstBuffer {
public:
    template <typename T>
    SharedConstBuffer(T * header) {
        operator()(header);
    }
    SharedConstBuffer(uint8_t * payload, std::size_t bytes) {
        operator()(payload, bytes);
    }
    SharedConstBuffer(const std::shared_ptr<uint8_t> & payload, std::size_t bytes) {
        operator()(payload, bytes);
    }
    template <typename T>
    SharedConstBuffer & operator()(T * header) {
        data.emplace_back(std::shared_ptr<T>(header));
        buffers.emplace_back(asio::buffer(header, sizeof(T)));
        return *this;
    }
    SharedConstBuffer & operator()(const std::shared_ptr<uint8_t> & payload, std::size_t bytes) {
        data.emplace_back(payload);
        buffers.emplace_back(asio::buffer(payload.get(), bytes));
        return *this;
    }
    SharedConstBuffer & operator()(uint8_t * payload, std::size_t bytes) {
        return operator()(std::shared_ptr<uint8_t>(payload, std::default_delete<uint8_t[]>()), bytes);
    }

    // Implement the ConstBufferSequence requirements.
    typedef asio::const_buffer value_type;
    typedef std::list<asio::const_buffer>::const_iterator const_iterator;
    const_iterator begin() const {
        return buffers.begin();
    }
    const_iterator end() const {
        return buffers.end();
    }

private:
    std::list<std::shared_ptr<void>> data;
    std::list<asio::const_buffer> buffers;
};

} // namespace flexvm

#endif // _SHAREDCONSTBUFFER_HPP_
