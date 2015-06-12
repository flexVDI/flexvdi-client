/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _TIMEOUTKILLER_HPP_
#define _TIMEOUTKILLER_HPP_

#include <functional>
#include <boost/asio.hpp>

namespace flexvm {

class TimeoutKiller {
public:
    TimeoutKiller(boost::asio::io_service & io) : io(io), timeout(io) {}
    TimeoutKiller(boost::asio::io_service & io, int ms) : io(io), timeout(io) {
        reprogram(ms);
    }
    ~TimeoutKiller() { timeout.cancel(); }

    void reprogram(int ms) {
        timeout.expires_from_now(boost::posix_time::milliseconds(ms));
        timeout.async_wait(std::bind(&TimeoutKiller::timedOut, this, std::placeholders::_1));
    }

private:
    boost::asio::io_service & io;
    boost::asio::deadline_timer timeout;

    void timedOut(const boost::system::error_code & error) {
        if (!error) io.stop();
    }
};

}

#endif // _TIMEOUTKILLER_HPP_
