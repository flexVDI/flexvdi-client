/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDIAGENT_HPP_
#define _FLEXVDIAGENT_HPP_

#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include "DispatcherRegistry.hpp"
#include "VirtioPort.hpp"
#include "LocalPipe.hpp"

namespace flexvm {

class FlexVDIGuestAgent {
public:
    static FlexVDIGuestAgent & singleton() {
        static FlexVDIGuestAgent instance;
        return instance;
    }

    int run();
    void stop();
    void setVirtioEndpoint(const std::string & name) {
        port.setEndpoint(name);
    }
    void setLocalEndpoint(const std::string & name) {
        pipe.setEndpoint(name);
    }
    Connection::Ptr spiceClient() {
        return port.spiceClient();
    }
    template <typename Handler, typename MsgClass, typename ... Rest>
    void registerMessageHandler(Handler & handler) {
        dregistry.registerMessageHandler<MsgClass, Handler>(handler);
        registerMessageHandler<Handler, Rest...>(handler);
    }
    template <typename Handler>
    void registerMessageHandler(Handler & handler) {}

private:
    FlexVDIGuestAgent() :
    port(io, dregistry.asMessageHandler()),
    pipe(io, dregistry.asMessageHandler()) {}

    boost::asio::io_service io;
    DispatcherRegistry dregistry;
    VirtioPort port;
    LocalPipe pipe;
};


template <typename Component, typename ... Messages>
class FlexVDIComponent {
public:
    typedef FlexVDIComponent<Component, Messages...> ComponentClass;

    static Component & singleton() {
        static Component instance;
        return instance;
    }

private:
    static int regVar;
    static int registerComponent() {
        FlexVDIGuestAgent::singleton().
        registerMessageHandler<Component, Messages...>(singleton());
        return 0;
    }
};

#define REGISTER_COMPONENT(Component) \
template<> int Component::ComponentClass::regVar = registerComponent()

} // namespace flexvm

#endif // _FLEXVDIAGENT_HPP_
