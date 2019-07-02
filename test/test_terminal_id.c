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

gchar * discover_terminal_id();

void test_terminal_id() {
    int i;

    // Test that the discover_terminal_id function returns a string
    // that looks like a MAC address and is always the same
    g_autofree gchar * terminal_id = discover_terminal_id();
    g_assert_true(terminal_id != NULL);

    g_auto(GStrv) terms = g_strsplit(terminal_id, ":", 0);
    g_assert_cmpint(g_strv_length(terms), ==, 6);
    for (i = 0; i < 6; ++i) {
        g_assert_true(terms[i] != NULL);
        g_assert_cmpint(strlen(terms[i]), ==, 2);
        g_assert_true(g_ascii_isxdigit(terms[i][0]));
        g_assert_true(g_ascii_isxdigit(terms[i][1]));
    }

    g_autofree gchar * terminal_id2 = discover_terminal_id();
    g_assert_cmpstr(terminal_id, ==, terminal_id2);
}

int main(int argc, char * argv[]) {
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/misc/terminalid", test_terminal_id);

    return g_test_run();
}
