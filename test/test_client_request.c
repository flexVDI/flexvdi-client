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
#include "src/client-request.h"


typedef struct _Fixture {
    GMainLoop * loop;
    ClientConf * conf;
    ClientRequest * req;
    GError * error;
    JsonNode * resp;
} Fixture;

static void f_setup(Fixture * f, gconstpointer user_data) {
    f->loop = g_main_loop_new(NULL, FALSE);
    f->conf = client_conf_new();
    // Use a generic JSON web service to test requests
    client_conf_set_host(f->conf, "httpbin.org");
    f->error = NULL;
}

static void f_teardown(Fixture * f, gconstpointer user_data) {
    if (f->error)
        g_error_free(f->error);
    g_object_unref(f->req);
    g_object_unref(f->conf);
    g_main_loop_unref(f->loop);
}


/*
 * On request finished, get the result and stop the main loop
 */
static void request_result(ClientRequest * req, gpointer user_data) {
    Fixture * f = (Fixture *)user_data;
    f->resp = client_request_get_result(f->req, &f->error);
    g_main_loop_quit(f->loop);
}


static void test_client_request_get(Fixture *f, gconstpointer user_data) {
    f->req = client_request_new(f->conf, "/get", request_result, f);

    g_main_loop_run(f->loop);

    g_assert_no_error(f->error);
    g_assert_true(JSON_NODE_HOLDS_OBJECT(f->resp));
    JsonObject * response = json_node_get_object(f->resp);
    g_assert_true(json_object_has_member(response, "url"));
    const gchar * url = json_object_get_string_member(response, "url");
    g_assert_cmpstr(url, ==, "https://httpbin.org/get");
}


static void test_client_request_get_fail(Fixture *f, gconstpointer user_data) {
    f->req = client_request_new(f->conf, "/", request_result, f);

    g_main_loop_run(f->loop);

    g_assert_error(f->error, JSON_PARSER_ERROR, JSON_PARSER_ERROR_INVALID_BAREWORD);
}


static void test_client_request_post(Fixture *f, gconstpointer user_data) {
    f->req = client_request_new_with_data(f->conf, "/post",
        "foobar", "foobar not secret", request_result, f);

    g_main_loop_run(f->loop);

    g_assert_no_error(f->error);
    g_assert_true(JSON_NODE_HOLDS_OBJECT(f->resp));
    JsonObject * response = json_node_get_object(f->resp);
    g_assert_true(json_object_has_member(response, "data"));
    const gchar * data = json_object_get_string_member(response, "data");
    g_assert_cmpstr(data, ==, "foobar");
}


int main(int argc, char * argv[]) {
    g_test_init(&argc, &argv, NULL);

    g_setenv("FLEXVDI_LOG_STDERR", "1", TRUE);
    g_setenv("FLEXVDI_FATAL_LEVEL", "0", TRUE);
    client_log_setup();
    client_log_set_log_levels("5");

    g_test_add("/client-request/get",
        Fixture, NULL, f_setup, test_client_request_get, f_teardown);

    g_test_add("/client-request/get-fail",
        Fixture, NULL, f_setup, test_client_request_get_fail, f_teardown);

    g_test_add("/client-request/post",
        Fixture, NULL, f_setup, test_client_request_post, f_teardown);

    // TODO: Don't know how to check if a request has been canceled,
    // because the main loop runs forever

    return g_test_run();
}
