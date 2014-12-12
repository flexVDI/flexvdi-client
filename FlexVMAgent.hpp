/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVMAGENT_HPP_
#define _FLEXVMAGENT_HPP_

#include <memory>
#include <asio.hpp>
#include "DispatcherRegistry.hpp"
#include "VirtioPort.hpp"
#include "CredentialsManager.hpp"

namespace flexvm {

class FlexVMAgent : public VirtioPort::Handler {
public:
    FlexVMAgent(int argc, char * argv[]) : port(io, *this) {
        parseOptions(argc, argv);
    }

    void parseOptions(int argc, char * argv[]);
    int run();
    virtual void handle(uint32_t type, const std::shared_ptr<uint8_t> & msgBuffer);

private:
    asio::io_service io;
    DispatcherRegistry dregistry;
    VirtioPort port;
    CredentialsManager creds;

    void registerHandlers();
};

} // namespace flexvm

#endif // _FLEXVMAGENT_HPP_
