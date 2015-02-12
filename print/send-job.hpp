/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _SEND_JOB_HPP_
#define _SEND_JOB_HPP_

#include <istream>
#include <string>

namespace flexvm {

bool sendJob(std::istream & pdfFile, const std::string & options);

} // namespace flexvm

#endif // _SEND_JOB_HPP_
