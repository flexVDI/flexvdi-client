#include <glib.h>

static void start_element(GMarkupParseContext * context, const gchar * element_name,
                          const gchar ** attribute_names, const gchar ** attribute_values,
                          gpointer user_data, GError ** error) {
    GKeyFile * new_file = (GKeyFile *)user_data;

    if (g_str_equal(element_name, "Server")) {
        const gchar * host, * port;
        if (g_markup_collect_attributes(element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_BOOLEAN, "isWebSocket", NULL,
                                        G_MARKUP_COLLECT_STRING, "name", &host,
                                        G_MARKUP_COLLECT_STRING, "port", &port, NULL)) {
            g_key_file_set_string(new_file, "General", "host", host);
            g_key_file_set_string(new_file, "General", "port", port);
        }
    } else if (g_str_equal(element_name, "Preferences")) {
        const gchar * username, * tid;
        gboolean fullscreen;
        if (g_markup_collect_attributes(element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_BOOLEAN, "fullScreen", &fullscreen,
                                        G_MARKUP_COLLECT_STRING, "lastUserName", &username,
                                        G_MARKUP_COLLECT_STRING, "terminalId", &tid, NULL)) {
            g_key_file_set_boolean(new_file, "Session", "fullscreen", fullscreen);
            g_key_file_set_string(new_file, "General", "username", username);
            g_key_file_set_string(new_file, "General", "terminal-id", tid);
        }
    } else if (g_str_equal(element_name, "Network")) {
        const gchar * host, * port, * username, * password;
        gboolean use_web_proxy;
        if (g_markup_collect_attributes(element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_BOOLEAN, "useWebProxy", &use_web_proxy,
                                        G_MARKUP_COLLECT_STRING, "WebProxyName", &host,
                                        G_MARKUP_COLLECT_STRING, "WebProxyPort", &port,
                                        G_MARKUP_COLLECT_STRING, "WebProxyUser", &username,
                                        G_MARKUP_COLLECT_STRING, "WebProxyPass", &password, NULL)) {
            if (!use_web_proxy) {
                return;
            } else if (g_str_equal(username, "")) {
                g_autofree gchar * proxy_uri = g_strdup_printf("http://%s:%s", host, port);
                g_key_file_set_string(new_file, "General", "proxy-uri", proxy_uri);
            } else {
                g_autofree gchar * proxy_uri = g_strdup_printf("http://%s:%s@%s:%s", username, password, host, port);
                g_key_file_set_string(new_file, "General", "proxy-uri", proxy_uri);
            }
        }
    }
}


static void parse_text(GMarkupParseContext * context, const gchar * text, gsize text_len,
                       gpointer user_data, GError ** error) {
    GKeyFile * new_file = (GKeyFile *)user_data;

    const GSList * stack = g_markup_parse_context_get_element_stack(context);
    if (stack && g_str_equal(stack->data, "name") && stack->next && g_str_equal(stack->next->data, "SharedPrinters")) {
        gsize length = 0, i;
        g_auto(GStrv) printers = g_key_file_get_string_list(new_file, "Devices", "share-printer", &length, NULL);
        g_auto(GStrv) new_printers = g_malloc_n(length + 2, sizeof(gchar *));
        for (i = 0; i < length; ++i)
            new_printers[i] = g_strdup(printers[i]);
        new_printers[length] = g_strndup(text, text_len);
        new_printers[length + 1] = NULL;
        g_key_file_set_string_list(new_file, "Devices", "share-printer", (const gchar **)new_printers, length + 1);
    }
}


static void parse_error(GMarkupParseContext * context, GError * error, gpointer user_data) {
    g_warning("Error parsing legacy settings file: %s", error->message);
}


gboolean load_legacy_config_file(GKeyFile * new_file) {
    GError * error = NULL;

    g_autofree gchar * legacy_file_name =
        g_build_filename(g_getenv("APPDATA"), "flexVDI Client 2.2", "config.xml", NULL);
    g_autofree gchar * contents = NULL;
    gsize length;
    if (!g_file_get_contents(legacy_file_name, &contents, &length, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning("Error loading legacy settings file: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    g_info("Reading legacy settings file at %s", legacy_file_name);

    // Parse XML file
    GMarkupParser parser = {
        .start_element = start_element,
        .end_element = NULL,
        .text = parse_text,
        .passthrough = NULL,
        .error = parse_error
    };
    g_autoptr(GMarkupParseContext) context =
        g_markup_parse_context_new(&parser, G_MARKUP_IGNORE_QUALIFIED, new_file, NULL);
    return g_markup_parse_context_parse(context, contents, length, NULL) &&
           g_markup_parse_context_end_parse(context, NULL);
}
