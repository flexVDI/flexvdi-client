/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#ifndef _CLIENT_LOG_H
#define _CLIENT_LOG_H

#include <glib.h>


/*
 * client_log_setup
 *
 * Setup logging, possibly getting the loglevel option from command line
 */
void client_log_setup(int argc, char * argv[]);

/*
 * client_log_set_log_levels
 *
 * Set the log levels. levels is a string associating a domain with a log level.
 * It has the format dom1:level1,dom2:level2,...
 * Domain can be any string representing a valid domain, or 'all' for all domains.
 * If omited, 'all' is assumed, so a single number is also a valid verbose string.
 * Levels go from 0 (Error) to 5 (Debug).
 */
void client_log_set_log_levels(const gchar * levels);


#endif /* _CLIENT_LOG_H */
