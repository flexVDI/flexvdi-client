/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <boost/test/unit_test.hpp>
#include "LocalPipe.hpp"
#include "TimeoutKiller.hpp"
using namespace std;
using namespace std::placeholders;
namespace asio = boost::asio;

#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
#include <windows.h>
#endif

namespace flexvm {

#define FIXTURE LocalPipe_fixture
struct FIXTURE {
    asio::io_service io;
    LocalPipe pipe;
    Connection::Ptr conn;
    TimeoutKiller timeout;

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
    static constexpr const char * pipeName = "/var/tmp/flexvdi_test_pipe";
    asio::local::stream_protocol::endpoint ep;
    asio::local::stream_protocol::socket sock;

    FIXTURE() : pipe(io, bind(&FIXTURE::handle, this, _1, _2)), timeout(io, 1000),
                ep(pipeName), sock(io) {
        ::unlink(pipeName);
        pipe.setEndpoint(pipeName);
        pipe.open();
        sock.connect(ep);
    }
#else
    static constexpr const char * pipeName = "\\\\.\\pipe\\flexvdi_test_pipe";
    HANDLE h;
    asio::windows::stream_handle sock;

    FIXTURE() : pipe(io, bind(&FIXTURE::handle, this, _1, _2)), timeout(io, 1000),
                sock(io) {
        pipe.setEndpoint(pipeName);
        pipe.open();
        h = ::CreateFileA(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                          OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        sock.assign(h);
    }
#endif
    ~FIXTURE() {
        ::unlink(pipeName);
    }

    void handle(const Connection::Ptr & src, const MessageBuffer & msg) {
        BOOST_CHECK_EQUAL(msg.header().type, 1);
        BOOST_CHECK_EQUAL(msg.header().size, 10);
        conn = src;
        io.stop();
    }
};


BOOST_FIXTURE_TEST_CASE(LocalPipe_testRead, FIXTURE) {
    // Write something to the socket
    asio::async_write<>(sock, MessageBuffer(1, 10),
                        [] (const boost::system::error_code &, std::size_t) {});
    io.run();
    BOOST_CHECK(conn.get() != nullptr);
}


BOOST_FIXTURE_TEST_CASE(LocalPipe_testWrite, FIXTURE) {
    // Write something to the socket
    asio::async_write<>(sock, MessageBuffer(1, 10),
                        [] (const boost::system::error_code &, std::size_t) {});
    io.run();
    io.reset();
    BOOST_REQUIRE(conn.get() != nullptr);
    conn->send(MessageBuffer(2, 10));
    FlexVDIMessageHeader header{ 0, 0 };
    asio::async_read<>(sock, boost::asio::buffer(&header, sizeof(header)),
                        [this] (const boost::system::error_code &, std::size_t) { io.stop(); });
    timeout.reprogram(1000);
    io.run();
    BOOST_CHECK_EQUAL(header.type, 2);
}

} // namespace flexvm
