/**
 * Copyright Flexible Software Solutions S.L. 2014
 *
 * This file defines certain types and functions of the standard C++ library
 * that are not implemented in some versions of GCC. For instance, MinGW's GCC 4.8
 * lacks std::thread and std::mutex
 **/

#ifndef _LIBSTDCXX_COMPAT_HPP_
#define _LIBSTDCXX_COMPAT_HPP_

#include "config.h"
#if !HAVE_STD_THREAD
#include <boost/thread.hpp>

namespace std {
    typedef boost::thread thread;
    typedef boost::mutex mutex;
    typedef boost::condition_variable_any condition_variable;
} // namespace std

#endif

#endif // _LIBSTDCXX_COMPAT_HPP_
