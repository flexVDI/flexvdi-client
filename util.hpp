/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _UTIL_HPP_
#define _UTIL_HPP_

#include <system_error>
#include <cerrno>

#define throw_if(cond, msg) \
    do { if (cond) throw std::system_error(errno, std::system_category(), msg); } while(0)

#endif // _UTIL_HPP_
