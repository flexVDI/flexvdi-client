/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <boost/test/unit_test.hpp>
#include "../util.hpp"
#include <iostream>
using namespace ::boost::unit_test;

test_suite* init_unit_test_suite(int, char * [])   {
    flexvm::Log::setLogOstream(&std::cout);
    assign_op(framework::master_test_suite().p_name.value,
              BOOST_TEST_STRINGIZE(flexvdi-guest-agent), 0);
    return nullptr;
}
