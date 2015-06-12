/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <boost/test/unit_test.hpp>
#include "Connection.hpp"
using namespace std;
using namespace std::placeholders;
namespace asio = boost::asio;

namespace flexvm {

class MockConnection : public Connection {
public:
    virtual bool isOpen() const { return true; }
    virtual void close() {}
    virtual void asyncRead(const asio::mutable_buffers_1 & buffers, AsyncHandler handler) {
        size_t size = asio::buffer_size(buffer);
        if (position == size) return;
        // Write something into buffers and call handler
        if (error) {
            handler(error, 0);
        } else {
            size_t read_size = asio::buffer_size(buffers);
            BOOST_REQUIRE_LE(read_size, size - position);
            copy_n(buffer.shareData().get() + position, read_size,
                   asio::buffer_cast<uint8_t *>(buffers));
            position += read_size;
            handler(error, read_size);
        }
    }
    virtual void asyncWrite(const MessageBuffer & sendBuffer, AsyncHandler handler) {
        BOOST_CHECK_EQUAL(buffer.getMsgData(), sendBuffer.getMsgData());
        handler(error, error ? 0 : buffer.size());
    }

    void testRead() { readNextMessage(); }

    MockConnection(MessageHandler h) : Connection(h), position(0), garbage(0) {}

    boost::system::error_code error;
    MessageBuffer buffer;
    size_t position;
    size_t garbage;
};


#define FIXTURE Connection_fixture
struct FIXTURE {
    shared_ptr<MockConnection> conn;
    bool handled;
    bool handledError;

    void handle(const Connection::Ptr & src, const MessageBuffer & msg) {
        BOOST_CHECK_EQUAL(src, conn);
        auto data = conn->buffer.shareData();
        FlexVDIMessageHeader * header = (FlexVDIMessageHeader *)(data.get() + conn->garbage);
        BOOST_CHECK_EQUAL(msg.header().size, header->size);
        BOOST_CHECK_EQUAL(msg.header().type, header->type);
        handled = true;
    }

    void handleError(const Connection::Ptr & src, const boost::system::error_code & error) {
        BOOST_CHECK_EQUAL(src, conn);
        BOOST_CHECK_EQUAL(error, conn->error);
        handledError = true;
    }

    FIXTURE() : handled(false), handledError(false) {
        conn = make_shared<MockConnection>(bind(&FIXTURE::handle, this, _1, _2));
    }
    ~FIXTURE() {
    }
};


BOOST_FIXTURE_TEST_CASE(Connection_send, FIXTURE) {
    bool sent = false;
    conn->buffer = MessageBuffer(1, 10);
    conn->send(conn->buffer, [&sent]() { sent = true; });
    BOOST_CHECK(sent);
}


BOOST_FIXTURE_TEST_CASE(Connection_sendError, FIXTURE) {
    bool sent = false;
    conn->buffer = MessageBuffer(1, 10);
    conn->error = asio::error::bad_descriptor;
    conn->registerErrorHandler(bind(&FIXTURE::handleError, this, _1, _2));
    conn->send(conn->buffer, [&sent]() { sent = true; });
    BOOST_CHECK(!sent);
    BOOST_CHECK(handledError);
}


BOOST_FIXTURE_TEST_CASE(Connection_read, FIXTURE) {
    conn->buffer = MessageBuffer(1, 10);
    conn->testRead();
    BOOST_CHECK(handled);
}


BOOST_FIXTURE_TEST_CASE(Connection_readError, FIXTURE) {
    conn->buffer = MessageBuffer(1, 10);
    conn->error = asio::error::bad_descriptor;
    conn->registerErrorHandler(bind(&FIXTURE::handleError, this, _1, _2));
    conn->testRead();
    BOOST_CHECK(!handled);
    BOOST_CHECK(handledError);
}


BOOST_FIXTURE_TEST_CASE(Connection_readGarbage, FIXTURE) {
    conn->buffer = MessageBuffer(1, 10);
    conn->garbage = 3;
    auto data = conn->buffer.shareData();
    FlexVDIMessageHeader * header = (FlexVDIMessageHeader *)(data.get() + conn->garbage);
    *header = conn->buffer.header();
    header->size = 7;
    conn->testRead();
    BOOST_CHECK(handled);
}


BOOST_FIXTURE_TEST_CASE(Connection_namedConnections, FIXTURE) {
    Connection::registerNamedConnection(conn, "test-connection");
    Connection::Ptr ptr = Connection::getNamedConnection("test-connection");
    BOOST_CHECK_EQUAL(conn.get(), ptr.get());
}

} // namespace flexvm
