/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <boost/test/unit_test.hpp>
#include "DispatcherRegistry.hpp"
using namespace std;

namespace flexvm {

struct MockBadMsg {
};


struct MockHandler : public FlexVDIComponent<MockHandler
,FlexVDICredentialsMsg
> {
    typedef MessagePtr<FlexVDICredentialsMsg> FlexVDICredentialsMsgPtr;
    typedef MessagePtr<MockBadMsg> MockBadMsgPtr;

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
    handler.registerComponent(dregistry);
    // The next line must raise a compilation error on MessageType<MockBadMsg>
//     dregistry.registerMessageHandler<MockHandler, MockBadMsg>(handler);
}


BOOST_FIXTURE_TEST_CASE(DispatcherRegistry_handleMsg, DispatcherRegistryFixture) {
    handler.registerComponent(dregistry);
    MessageBuffer buffer(FLEXVDI_CREDENTIALS, 0);
    dregistry.handleMessage(Connection::Ptr(), buffer);
    BOOST_CHECK(handler.handled);
}


BOOST_FIXTURE_TEST_CASE(DispatcherRegistry_handleMsgFail, DispatcherRegistryFixture) {
    handler.registerComponent(dregistry);
    MessageBuffer buffer(FLEXVDI_CREDENTIALS, 0);
    dregistry.handleMessage(Connection::Ptr(), MessageBuffer(23465, 0));
    BOOST_CHECK(!handler.handled);
    dregistry.handleMessage(Connection::Ptr(), MessageBuffer(-1, 0));
    BOOST_CHECK(!handler.handled);
    dregistry.handleMessage(Connection::Ptr(), MessageBuffer(1, 0));
    BOOST_CHECK(!handler.handled);
}

} // namespace flexvm
