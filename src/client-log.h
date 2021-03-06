/*
    Copyright (C) 2014-2018 Flexible Software Solutions S.L.U.

    This file is part of flexVDI Client.

    flexVDI Client is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    flexVDI Client is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flexVDI Client. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _CLIENT_LOG_H
#define _CLIENT_LOG_H

#include <glib.h>


/*
 * client_log_setup
 *
 * Setup logging file
 */
void client_log_setup();

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

/*
 * client_log_get_level_for_domain
 *
 * Get the log level for a domain.
 */
int client_log_get_level_for_domain(const gchar * domain);

/*
 * print_to_stdout
 *
 * A GLib print handler that makes sure messages are printed to stdout.
 */
void print_to_stdout(const gchar * string);


#endif /* _CLIENT_LOG_H */
