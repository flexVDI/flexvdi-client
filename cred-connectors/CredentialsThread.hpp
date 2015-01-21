/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _CREDENTIALSTHREAD_HPP_
#define _CREDENTIALSTHREAD_HPP_

#include <memory>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

namespace flexvm {

class CredentialsThread {
public:
    class Listener {
    public:
        virtual void credentialsChanged(const char * username, const char * password,
                                        const char * domain) = 0;
    };

    CredentialsThread(Listener & l) : listener(l), pipe(io) {}
    ~CredentialsThread() { stop(); }

    void requestCredentials();
    void stop();

private:
    Listener & listener;
    boost::thread thread;
    boost::asio::io_service io;
    std::shared_ptr<uint8_t> buffer;
    std::size_t bufferSize;
    boost::system::error_code lastError;
#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
    typedef boost::asio::windows::stream_handle pipe_t;
#else
    typedef boost::asio::local::stream_protocol::socket pipe_t;
#endif
    pipe_t pipe;

    bool openPipe();
    void readCredentials();
    void requestSent(const boost::system::error_code & error, std::size_t bytes);
    void headerRead(const boost::system::error_code & error, std::size_t bytes);
    void bodyRead(const boost::system::error_code & error, std::size_t bytes);
};

} // namespace flexvm

#endif // _CREDENTIALSTHREAD_HPP_
