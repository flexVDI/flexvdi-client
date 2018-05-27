/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include "configuration.h"

struct _ClientConf {
    GObject parent;
    GOptionEntry * cmdline_entries;
    GKeyFile * file;
    gboolean version;
    gchar * host;
    gint port;
    gboolean fullscreen;
    const gchar ** serial_params;
    gboolean disable_printing;
};

G_DEFINE_TYPE(ClientConf, client_conf, G_TYPE_OBJECT);

GOptionEntry * client_conf_get_cmdline_entries(ClientConf * conf) {
    return conf->cmdline_entries;
}

gboolean client_conf_show_version(ClientConf * conf) {
    return conf->version;
}

const gchar * client_conf_get_host(ClientConf * conf) {
    return conf->host;
}

gint client_conf_get_port(ClientConf * conf) {
    return conf->port;
}

gboolean client_conf_get_fullscreen(ClientConf * conf) {
    return conf->fullscreen;
}

const gchar ** client_conf_get_serial_params(ClientConf * conf) {
    return conf->serial_params;
}

gboolean client_conf_get_disable_printing(ClientConf * conf) {
    return conf->disable_printing;
}

static const GOptionEntry cmdline_entries[] = {
    // { long_name, short_name, flags, arg, arg_data, description, arg_description },
    { "version", 0, 0, G_OPTION_ARG_NONE, NULL,
      "Show version and exit", NULL },
    { "host", 'h', 0, G_OPTION_ARG_STRING, NULL,
      "Connection host address", NULL },
    { "port", 'p', 0, G_OPTION_ARG_INT, NULL,
      "Connection port (default 443)", NULL },
    { "fullscreen", 'f', 0, G_OPTION_ARG_NONE, NULL,
      "Show desktop in fullscreen mode", NULL },
    { "flexvdi-serial-port", 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL,
      "Add serial port redirection. "
      "The Nth use of this option is attached to channel serialredirN. "
      "Example: /dev/ttyS0,9600,8N1", "<device,speed,mode>" },
    { "flexvdi-disable-printing", 0, 0, G_OPTION_ARG_NONE, NULL,
      "Disable printing support", NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};

static void client_conf_load(ClientConf * conf) {
    GError * error = NULL;
    g_autofree gchar * config_filename = g_build_filename(
        g_get_user_config_dir(),
        "flexVDI Client",
        "settings.ini",
        NULL
    );

    if (!g_key_file_load_from_file(conf->file, config_filename,
            G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning("Error loading settings file: %s", error->message);
        return;
    }

    gchar * host_val = g_key_file_get_string(conf->file, "General", "host", NULL);
    if (host_val != NULL)
        conf->host = host_val;

    gint port_val = g_key_file_get_integer(conf->file, "General", "port", NULL);
    if (port_val != 0)
        conf->port = port_val;

    gboolean fs_val = g_key_file_get_boolean(conf->file, "General", "fullscreen", &error);
    if (!error)
        conf->fullscreen = fs_val;
    else
        g_error_free(error);

    gboolean print_val = g_key_file_get_boolean(conf->file, "General", "disable_printing", &error);
    if (!error)
        conf->disable_printing = print_val;
    else
        g_error_free(error);
}

static void client_conf_init(ClientConf * conf) {
    conf->version = FALSE;
    conf->host = NULL;
    conf->port = 443;
    conf->fullscreen = FALSE;
    conf->serial_params = NULL;
    conf->disable_printing = FALSE;
    conf->cmdline_entries = g_memdup(cmdline_entries, sizeof(cmdline_entries));
    conf->file = g_key_file_new();
    client_conf_load(conf);

    int i = 0;
    conf->cmdline_entries[i++].arg_data = &conf->version;
    conf->cmdline_entries[i++].arg_data = &conf->host;
    conf->cmdline_entries[i++].arg_data = &conf->port;
    conf->cmdline_entries[i++].arg_data = &conf->fullscreen;
    conf->cmdline_entries[i++].arg_data = &conf->serial_params;
    conf->cmdline_entries[i++].arg_data = &conf->disable_printing;
}

static void client_conf_finalize(GObject * obj) {
    ClientConf * conf = CLIENT_CONF(obj);
    g_free(conf->cmdline_entries);
    g_free(conf->host);
    g_free(conf->serial_params);
    g_key_file_free(conf->file);
    G_OBJECT_CLASS(client_conf_parent_class)->finalize(obj);
}

static void client_conf_class_init(ClientConfClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->finalize = client_conf_finalize;
}

ClientConf * client_conf_new(void) {
    return g_object_new(CLIENT_CONF_TYPE, NULL);
}
