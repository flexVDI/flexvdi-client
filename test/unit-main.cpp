/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#define BOOST_TEST_MODULE flexvdi-guest-agent
#include <boost/test/unit_test.hpp>
#include "../util.hpp"
#include <iostream>

static int setLog() {
    flexvm::Log::setLogOstream(&std::cout);
    return 0;
}
static int foo = setLog();
