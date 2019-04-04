#include <gio/gio.h>

gboolean load_legacy_config_file(GKeyFile * new_file) {
    GError * error = NULL;
    char * line;

    g_autoptr(GFile) file =
        g_file_new_build_filename(g_getenv("HOME"), ".flexvdi", "flexvdi-client.conf", NULL);
    g_autoptr(GFileInputStream) fistream = g_file_read(file, NULL, &error);
    if (error) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning("Error loading legacy settings file: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    g_info("Reading legacy settings file at %s", g_file_get_path(file));

    // Read file line by line
    g_autoptr(GDataInputStream) distream = g_data_input_stream_new(G_INPUT_STREAM(fistream));
    for (; (line = g_data_input_stream_read_line_utf8(distream, NULL, NULL, NULL)); g_free(line)) {
        char * c = line;
        // Skip blanks
        while (*c == ' ') ++c;
        // Ignore comments and blank lines
        if (*c == '#' || !*c) continue;
        // Read key
        char * key = c;
        while (*c && *c != ' ') ++c;
        if (!*c) continue;
        *(c++) = '\0';
        // Skip equal sign
        while (*c == ' ' || *c == '=') ++c;
        if (!*c) continue;
        // Read value
        char * value = c;
        g_debug("Parsed key=%s, value=%s", key, value);

        if (g_str_equal(key, "manager_ip")) {
            g_key_file_set_string(new_file, "General", "host", value);
        } else if (g_str_equal(key, "spice_extra_args")) {
            if (strstr(value, "-f"))
                g_key_file_set_boolean(new_file, "Session", "fullscreen", TRUE);
        } else if (g_str_equal(key, "username")) {
            g_key_file_set_string(new_file, "General", "username", value);
        } else if (g_str_equal(key, "password")) {
            g_key_file_set_string(new_file, "General", "password", value);
        } else if (g_str_equal(key, "desktop")) {
            g_key_file_set_string(new_file, "General", "desktop", value);
        } else if (g_str_equal(key, "terminal_id")) {
            g_key_file_set_string(new_file, "General", "terminal-id", value);
        }
    }

    return TRUE;
}
