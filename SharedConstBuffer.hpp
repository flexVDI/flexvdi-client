/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _SHAREDCONSTBUFFER_HPP_
#define _SHAREDCONSTBUFFER_HPP_

#include <memory>
#include <list>
#include <boost/asio.hpp>

namespace flexvm {

class SharedConstBuffer {
public:
    template <typename T>
    SharedConstBuffer(T * header) {
        operator()(header);
    }
    template <typename T>
    SharedConstBuffer(const std::shared_ptr<T> & payload, std::size_t bytes) {
        operator()(payload, bytes);
    }
    template <typename T>
    SharedConstBuffer & operator()(T * header) {
        data.emplace_back(std::shared_ptr<T>(header));
        buffers.emplace_back(boost::asio::buffer(header, sizeof(T)));
        return *this;
    }
    template <typename T>
    SharedConstBuffer & operator()(const std::shared_ptr<T> & payload, std::size_t bytes) {
        data.emplace_back(payload);
        buffers.emplace_back(boost::asio::buffer(payload.get(), bytes));
        return *this;
    }
    SharedConstBuffer & operator()(const SharedConstBuffer & buffer) {
        data.insert(data.end(), buffer.data.begin(), buffer.data.end());
        buffers.insert(buffers.end(), buffer.buffers.begin(), buffer.buffers.end());
        return *this;
    }

    std::size_t size() const {
        std::size_t result = 0;
        for (auto & buffer : buffers) {
            result += boost::asio::buffer_size(buffer);
        }
        return result;
    }

    // Implement the ConstBufferSequence requirements.
    typedef boost::asio::mutable_buffer value_type;
    typedef std::list<value_type>::const_iterator const_iterator;
    const_iterator begin() const {
        return buffers.begin();
    }
    const_iterator end() const {
        return buffers.end();
    }

    typedef std::list<value_type>::iterator iterator;
    iterator begin() {
        return buffers.begin();
    }
    iterator end() {
        return buffers.end();
    }

private:
    std::list<std::shared_ptr<void>> data;
    std::list<value_type> buffers;
};

} // namespace flexvm

#endif // _SHAREDCONSTBUFFER_HPP_
