/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include <spice-client-gtk.h>
#include "configuration.h"

struct _ClientConf {
    GObject parent;
    GOptionEntry * main_options;
    GKeyFile * file;
    gboolean version;
    gchar * host;
    gchar * port;
    gchar * username;
    gchar * password;
    gchar * desktop;
    gboolean fullscreen;
    const gchar ** serial_params;
    gboolean disable_printing;
    gchar * terminal_id;
};

G_DEFINE_TYPE(ClientConf, client_conf, G_TYPE_OBJECT);

static void client_conf_finalize(GObject * obj);

static void client_conf_class_init(ClientConfClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->finalize = client_conf_finalize;
}

static void client_conf_load(ClientConf * conf);

static void client_conf_init(ClientConf * conf) {
    GOptionEntry main_options[] = {
        // { long_name, short_name, flags, arg, arg_data, description, arg_description },
        { "version", 0, 0, G_OPTION_ARG_NONE, &conf->version,
        "Show version and exit", NULL },
        { "host", 'h', 0, G_OPTION_ARG_STRING, &conf->host,
        "Connection host address", NULL },
        { "port", 'p', 0, G_OPTION_ARG_STRING, &conf->port,
        "Connection port (default 443)", NULL },
        { "username", 'u', 0, G_OPTION_ARG_STRING, &conf->username,
        "User name", NULL },
        { "password", 'w', 0, G_OPTION_ARG_STRING, &conf->password,
        "Password", NULL },
        { "desktop", 'd', 0, G_OPTION_ARG_STRING, &conf->desktop,
        "Desktop name to connect to", NULL },
        { "fullscreen", 'f', 0, G_OPTION_ARG_NONE, &conf->fullscreen,
        "Show desktop in fullscreen mode", NULL },
        { "flexvdi-serial-port", 0, 0, G_OPTION_ARG_STRING_ARRAY, &conf->serial_params,
        "Add serial port redirection. "
        "The Nth use of this option is attached to channel serialredirN. "
        "Example: /dev/ttyS0,9600,8N1", "<device,speed,mode>" },
        { "flexvdi-disable-printing", 0, 0, G_OPTION_ARG_NONE, &conf->disable_printing,
        "Disable printing support", NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };

    conf->version = FALSE;
    conf->host = NULL;
    conf->port = NULL;
    conf->username = NULL;
    conf->password = NULL;
    conf->desktop = NULL;
    conf->fullscreen = FALSE;
    conf->serial_params = NULL;
    conf->disable_printing = FALSE;
    conf->terminal_id = NULL;
    conf->main_options = g_memdup(main_options, sizeof(main_options));
    conf->file = g_key_file_new();
    client_conf_load(conf);
}

static void client_conf_finalize(GObject * obj) {
    ClientConf * conf = CLIENT_CONF(obj);
    g_free(conf->main_options);
    g_free(conf->host);
    g_free(conf->port);
    g_free(conf->username);
    g_free(conf->password);
    g_free(conf->desktop);
    g_free(conf->serial_params);
    g_free(conf->terminal_id);
    g_key_file_free(conf->file);
    G_OBJECT_CLASS(client_conf_parent_class)->finalize(obj);
}

ClientConf * client_conf_new(void) {
    return g_object_new(CLIENT_CONF_TYPE, NULL);
}

void client_conf_set_application_options(ClientConf * conf, GApplication * app) {
    g_application_add_main_option_entries(app, conf->main_options);
    g_application_add_option_group(app, spice_get_option_group());
}

gboolean client_conf_show_version(ClientConf * conf) {
    return conf->version;
}

const gchar * client_conf_get_host(ClientConf * conf) {
    return conf->host;
}

const gchar * client_conf_get_port(ClientConf * conf) {
    return conf->port;
}

const gchar * client_conf_get_username(ClientConf * conf) {
    return conf->username;
}

const gchar * client_conf_get_password(ClientConf * conf) {
    return conf->password;
}

const gchar * client_conf_get_desktop(ClientConf * conf) {
    return conf->desktop;
}

gchar * client_conf_get_connection_uri(ClientConf * conf, const gchar * path) {
    if (!conf->host) return NULL;
    if (!conf->port)
        return g_strdup_printf("https://%s%s", conf->host, path);
    else
        return g_strdup_printf("https://%s:%s%s", conf->host, conf->port, path);
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

gchar * discover_terminal_id();

const gchar * client_conf_get_terminal_id(ClientConf * conf) {
    if (!conf->terminal_id) {
        conf->terminal_id = discover_terminal_id();
        if (conf->terminal_id[0] == '\0') {
            // TODO: Random terminal id
        }
        // TODO: Save terminal id
    }
    return conf->terminal_id;
}

void client_conf_set_host(ClientConf * conf, const gchar * host) {
    conf->host = g_strdup(host);
}

void client_conf_set_port(ClientConf * conf, const gchar * port) {
    conf->port = port ? g_strdup(port) : NULL;
}

void client_conf_set_fullscreen(ClientConf * conf, gboolean fs) {
    conf->fullscreen = fs;
}

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

    struct { const gchar * group, * key; gchar ** value; } string_params[] = {
        { "General", "terminal_id", &conf->terminal_id },
        { "General", "host", &conf->host },
        { "General", "port", &conf->port }
    };

    struct { const gchar * group, * key; gboolean * value; } bool_params[] = {
        { "General", "fullscreen", &conf->fullscreen },
        { "General", "disable_printing", &conf->disable_printing }
    };

    for (int i = 0; i < G_N_ELEMENTS(string_params); ++i) {
        gchar * val = g_key_file_get_string(conf->file, string_params[i].group,
                                            string_params[i].key, NULL);
        if (val)
            *string_params[i].value = val;
    }

    for (int i = 0; i < G_N_ELEMENTS(bool_params); ++i) {
        gboolean val = g_key_file_get_boolean(conf->file, bool_params[i].group,
                                              bool_params[i].key, &error);
        if (!error)
            *bool_params[i].value = val;
        else
            g_clear_error(&error);
    }
}
