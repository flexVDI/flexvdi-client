/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _DISPATCHERREGISTRY_HPP_
#define _DISPATCHERREGISTRY_HPP_

#include <list>
#include <memory>
#include "Connection.hpp"

namespace flexvm {

class BaseMessageDispatcher {
public:
    virtual ~BaseMessageDispatcher() {}
    virtual void dispatch(const Connection::Ptr & src,
                          const MessageBuffer & msgBuffer) = 0;
};

template <typename MsgClass, typename Handler>
class MessageDispatcher : public BaseMessageDispatcher {
public:
    MessageDispatcher(Handler & handler) : handler(handler) {}

protected:
    Handler & handler;

    virtual void dispatch(const Connection::Ptr & src,
                          const MessageBuffer & msgBuffer) {
        handler.handle(src, msgBuffer.getMessagePtr<MsgClass>());
    }
};


class DispatcherRegistry {
public:
    DispatcherRegistry();

    template <typename Handler, typename MsgClass, typename ... Rest>
    void registerMessageHandler(Handler & handler) {
        dispatchers[MessageType<MsgClass>::type].emplace_back(
            new MessageDispatcher<MsgClass, Handler>(handler));
        registerMessageHandler<Handler, Rest...>(handler);
    }
    template <typename Handler>
    void registerMessageHandler(Handler & handler) {}

    void handleMessage(const Connection::Ptr & src, const MessageBuffer & msgBuffer);

    Connection::MessageHandler asMessageHandler() {
        return std::bind(&DispatcherRegistry::handleMessage, this,
                         std::placeholders::_1, std::placeholders::_2);
    }

private:
    template <typename MsgClass> struct MessageType {
        static const uint32_t type;
    };

    std::unique_ptr<std::list<std::unique_ptr<BaseMessageDispatcher>>[]> dispatchers;
};


template <typename Component, typename ... Messages>
class FlexVDIComponent {
public:
    typedef FlexVDIComponent<Component, Messages...> ComponentClass;

    static Component & singleton() {
        static Component instance;
        return instance;
    }

    int registerComponent(DispatcherRegistry & dregistry) {
        Component * This = static_cast<Component *>(this);
        dregistry.registerMessageHandler<Component, Messages...>(*This);
        return 0;
    }

private:
    static int regVar;
};

#define REGISTER_COMPONENT_WITH_DISPATCHER(Component, Dispatcher) \
template<> int Component::ComponentClass::regVar = \
    Component::singleton().registerComponent(Dispatcher)

} // namespace flexvm

#endif // _DISPATCHERREGISTRY_HPP_
