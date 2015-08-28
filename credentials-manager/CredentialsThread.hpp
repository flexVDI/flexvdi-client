/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _CREDENTIALSTHREAD_HPP_
#define _CREDENTIALSTHREAD_HPP_

#include <memory>
#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "../LocalPipeClient.hpp"

namespace flexvm {

class CredentialsThread {
public:
    class Listener {
    public:
        virtual void credentialsChanged(const char * username, const char * password,
                                        const char * domain) = 0;
    };

    CredentialsThread(Listener & l) :
        listener(l), endpointName(LocalPipeClient::defaultPipeName) {}
    ~CredentialsThread() { stop(); }

    void requestCredentials();
    void stop();
    void setEndpoint(const std::string & name) {
        endpointName = name;
    }

private:
    Listener & listener;
    boost::thread thread;
    boost::asio::io_service io;
    boost::system::error_code lastError;
    std::string endpointName;
    LocalPipeClient::Ptr pipe;

    void readCredentials();
    void handleMessage(const Connection::Ptr &, const MessageBuffer & msg);
};

} // namespace flexvm

#endif // _CREDENTIALSTHREAD_HPP_
