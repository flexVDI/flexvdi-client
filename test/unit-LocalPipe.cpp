/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <boost/test/unit_test.hpp>
#include "LocalPipe.hpp"
using namespace std;
using namespace std::placeholders;
namespace asio = boost::asio;

namespace flexvm {

#define FIXTURE LocalPipe_fixture
struct FIXTURE {
    static constexpr const char * pipeName = "/var/tmp/flexvdi_test_pipe";
    asio::io_service io;
    LocalPipe pipe;
    asio::local::stream_protocol::endpoint ep;
    Connection::Ptr conn;

    void handle(const Connection::Ptr & src, const MessageBuffer & msg) {
        BOOST_CHECK_EQUAL(msg.header().type, 1);
        BOOST_CHECK_EQUAL(msg.header().size, 10);
        conn = src;
        io.stop();
    }

    FIXTURE() : pipe(io, bind(&FIXTURE::handle, this, _1, _2)), ep(pipeName) {
        ::unlink(pipeName);
        pipe.setEndpoint(pipeName);
        pipe.open();
    }
    ~FIXTURE() {
        ::unlink(pipeName);
    }
};


BOOST_FIXTURE_TEST_CASE(LocalPipe_testRead, FIXTURE) {
    // Write something to the socket
    asio::local::stream_protocol::socket sock(io);
    sock.connect(ep);
    asio::async_write<>(sock, MessageBuffer(1, 10),
                        [] (const boost::system::error_code &, std::size_t) {});
    io.run();
    BOOST_CHECK(conn.get() != nullptr);
}


BOOST_FIXTURE_TEST_CASE(LocalPipe_testWrite, FIXTURE) {
    // Write something to the socket
    asio::local::stream_protocol::socket sock(io);
    sock.connect(ep);
    asio::async_write<>(sock, MessageBuffer(1, 10),
                        [] (const boost::system::error_code &, std::size_t) {});
    io.run();
    io.reset();
    BOOST_REQUIRE(conn.get() != nullptr);
    conn->send(MessageBuffer(2, 10));
    FlexVDIMessageHeader header{ 0, 0 };
    asio::async_read<>(sock, boost::asio::buffer(&header, sizeof(header)),
                        [this] (const boost::system::error_code &, std::size_t) { io.stop(); });
    io.run();
    BOOST_CHECK_EQUAL(header.type, 2);
}

} // namespace flexvm
