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
                          const std::shared_ptr<uint8_t> & msgBuffer) = 0;
};

template <typename MsgClass, typename Handler>
class MessageDispatcher : public BaseMessageDispatcher {
public:
    MessageDispatcher(Handler & handler) : handler(handler) {}

protected:
    Handler & handler;

    virtual void dispatch(const Connection::Ptr & src,
                          const std::shared_ptr<uint8_t> & msgBuffer) {
        std::shared_ptr<MsgClass> msg(msgBuffer, (MsgClass *)msgBuffer.get());
        handler.handle(src, msg);
    }
};

class DispatcherRegistry {
public:
    DispatcherRegistry();

    template <typename MsgClass, typename Handler>
    void registerMessageHandler(Handler & handler) {
        dispatchers[MessageType<MsgClass>::type].emplace_back(
            new MessageDispatcher<MsgClass, Handler>(handler));
    }

    void handleMessage(const Connection::Ptr & src, uint32_t type,
                       const std::shared_ptr<uint8_t> & msgBuffer);

    Connection::MessageHandler asMessageHandler() {
        using namespace std::placeholders;
        return std::bind(&DispatcherRegistry::handleMessage, this, _1, _2, _3);
    }

private:
    template <typename MsgClass> struct MessageType {
        static const uint32_t type;
    };

    std::unique_ptr<std::list<std::unique_ptr<BaseMessageDispatcher>>[]> dispatchers;
};

} // namespace flexvm

#endif // _DISPATCHERREGISTRY_HPP_
