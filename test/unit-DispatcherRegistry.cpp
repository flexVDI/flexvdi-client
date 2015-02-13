/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <boost/test/unit_test.hpp>
#include "DispatcherRegistry.hpp"
using namespace std;

namespace flexvm {

struct MockBadMsg {
};


struct MockHandler {
    typedef std::shared_ptr<FlexVDICredentialsMsg> FlexVDICredentialsMsgPtr;
    typedef std::shared_ptr<MockBadMsg> MockBadMsgPtr;

    void handle(const Connection::Ptr & src, const FlexVDICredentialsMsgPtr & msg) {
        handled = true;
    }
    void handle(const Connection::Ptr & src, const MockBadMsgPtr & msg) {
    }

    bool handled;
    MockHandler() : handled(false) {}
};


struct DispatcherRegistryFixture {
    DispatcherRegistry dregistry;
    MockHandler handler;

    DispatcherRegistryFixture() {}
    ~DispatcherRegistryFixture() {
    }
};


BOOST_FIXTURE_TEST_CASE(DispatcherRegistry_registerHandler, DispatcherRegistryFixture) {
    dregistry.registerMessageHandler<FlexVDICredentialsMsg>(handler);
    // The next line must raise a compilation error on MessageType<MockBadMsg>
//     dregistry.registerMessageHandler<MockBadMsg>(handler);
}


BOOST_FIXTURE_TEST_CASE(DispatcherRegistry_handleMsg, DispatcherRegistryFixture) {
    dregistry.registerMessageHandler<FlexVDICredentialsMsg>(handler);
    auto msg = make_shared<FlexVDICredentialsMsg>();
    shared_ptr<uint8_t> buffer(msg, (uint8_t *)msg.get());
    dregistry.handleMessage(Connection::Ptr(), FLEXVDI_CREDENTIALS, buffer);
    BOOST_CHECK(handler.handled);
}


BOOST_FIXTURE_TEST_CASE(DispatcherRegistry_handleMsgFail, DispatcherRegistryFixture) {
    dregistry.registerMessageHandler<FlexVDICredentialsMsg>(handler);
    shared_ptr<uint8_t> buffer;
    dregistry.handleMessage(Connection::Ptr(), 23465, buffer);
    BOOST_CHECK(!handler.handled);
    dregistry.handleMessage(Connection::Ptr(), -1, buffer);
    BOOST_CHECK(!handler.handled);
    dregistry.handleMessage(Connection::Ptr(), 1, buffer);
    BOOST_CHECK(!handler.handled);
}

} // namespace flexvm
