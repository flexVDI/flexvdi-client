/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <boost/test/unit_test.hpp>
#include "MessageBuffer.hpp"
using namespace std;

namespace flexvm {

#define FIXTURE MessageBuffer_fixture
struct FIXTURE {
    FIXTURE() {}
    ~FIXTURE() {
    }
};


BOOST_FIXTURE_TEST_CASE(MessageBuffer_ctor, FIXTURE) {
    MessageBuffer mb1;
    BOOST_CHECK(!mb1.isValid());
    FlexVDIMessageHeader header{ .type = 1, .size = 10 };
    MessageBuffer mb2(header);
    BOOST_REQUIRE(mb2.isValid());
    BOOST_CHECK_EQUAL(mb2.size(), 10 + sizeof(FlexVDIMessageHeader));
    BOOST_CHECK_EQUAL(mb2.header().type, 1);
    MessageBuffer mb3(1, 12);
    BOOST_REQUIRE(mb3.isValid());
    BOOST_CHECK_EQUAL(mb3.size(), 12 + sizeof(FlexVDIMessageHeader));
    auto p = mb3.getMessagePtr<FlexVDICredentialsMsg>();
    BOOST_REQUIRE(p.get() != nullptr);
    BOOST_CHECK_EQUAL((void *)p.get(), mb3.getMsgData());
    MessageBuffer mb4(p);
    BOOST_REQUIRE(mb4.isValid());
    BOOST_CHECK_EQUAL(mb4.size(), 12 + sizeof(FlexVDIMessageHeader));
    BOOST_CHECK_EQUAL(mb4.getMsgData(), mb3.getMsgData());
}

} // namespace flexvm
