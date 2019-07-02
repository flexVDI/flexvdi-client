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

#include <glib.h>
#include "src/client-log.h"


void test_client_log() {
    // Test that log configuration strings work

    client_log_set_log_levels("4");
    g_assert_true(client_log_get_level_for_domain("foo") == G_LOG_LEVEL_INFO);

    client_log_set_log_levels("foo:4,5");
    g_assert_true(client_log_get_level_for_domain("foo") == G_LOG_LEVEL_INFO);
    g_assert_true(client_log_get_level_for_domain("bar") == G_LOG_LEVEL_DEBUG);

    client_log_set_log_levels("foo:4");
    g_assert_true(client_log_get_level_for_domain("foo") == G_LOG_LEVEL_INFO);
    g_assert_true(client_log_get_level_for_domain("bar") == G_LOG_LEVEL_MESSAGE);

    client_log_set_log_levels("foo:4,3,bar:2");
    g_assert_true(client_log_get_level_for_domain("foo") == G_LOG_LEVEL_INFO);
    g_assert_true(client_log_get_level_for_domain("bar") == G_LOG_LEVEL_WARNING);
    g_assert_true(client_log_get_level_for_domain("baz") == G_LOG_LEVEL_MESSAGE);
    g_assert_true(g_getenv("SPICE_DEBUG") == NULL);

    client_log_set_log_levels("GSpice:5");
    g_assert_cmpstr(g_getenv("SPICE_DEBUG"), ==, "1");
}

int main(int argc, char * argv[]) {
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/misc/client_log", test_client_log);

    return g_test_run();
}
