/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include <spice-client-gtk.h>
#include "configuration.h"

struct _ClientConf {
    GObject parent;
    GOptionEntry * main_options, * session_options, * device_options;
    GKeyFile * file;
    gboolean version;
    gchar * host;
    gchar * port;
    gchar * username;
    gchar * password;
    gchar * desktop;
    gboolean fullscreen;
    gchar ** serial_params;
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
        { "host", 'h', 0, G_OPTION_ARG_STRING, &conf->host,
        "Connection host address", NULL },
        { "port", 'p', 0, G_OPTION_ARG_STRING, &conf->port,
        "Connection port (default 443)", NULL },
        { "username", 'u', 0, G_OPTION_ARG_STRING, &conf->username,
        "User name", NULL },
        { "password", 'w', 0, G_OPTION_ARG_STRING, &conf->password,
        "Password", NULL },
        { "terminal_id", 0, 0, G_OPTION_ARG_STRING, &conf->terminal_id,
        "Use a given Terminal ID instead of calculating it automatically", NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };

    GOptionEntry session_options[] = {
        { "desktop", 'd', 0, G_OPTION_ARG_STRING, &conf->desktop,
        "Desktop name to connect to", NULL },
        { "fullscreen", 'f', 0, G_OPTION_ARG_NONE, &conf->fullscreen,
        "Show desktop in fullscreen mode", NULL },
        { "flexvdi-disable-printing", 0, 0, G_OPTION_ARG_NONE, &conf->disable_printing,
        "Disable printing support", NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };

    GOptionEntry device_options[] = {
        { "flexvdi-serial-port", 0, 0, G_OPTION_ARG_STRING_ARRAY, &conf->serial_params,
        "Add serial port redirection. "
        "The Nth use of this option is attached to channel serialredirN. "
        "Example: /dev/ttyS0,9600,8N1", "<device,speed,mode>" },
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
    conf->session_options = g_memdup(session_options, sizeof(session_options));
    conf->device_options = g_memdup(device_options, sizeof(device_options));
    conf->file = g_key_file_new();
    client_conf_load(conf);
}

static void client_conf_finalize(GObject * obj) {
    ClientConf * conf = CLIENT_CONF(obj);
    g_free(conf->main_options);
    g_free(conf->session_options);
    g_free(conf->device_options);
    g_free(conf->host);
    g_free(conf->port);
    g_free(conf->username);
    g_free(conf->password);
    g_free(conf->desktop);
    g_strfreev(conf->serial_params);
    g_free(conf->terminal_id);
    g_key_file_free(conf->file);
    G_OBJECT_CLASS(client_conf_parent_class)->finalize(obj);
}

ClientConf * client_conf_new(void) {
    return g_object_new(CLIENT_CONF_TYPE, NULL);
}

void client_conf_set_application_options(ClientConf * conf, GApplication * app) {
    GOptionEntry version_option[] = {
        { "version", 0, 0, G_OPTION_ARG_NONE, &conf->version,
        "Show version and exit", NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };
    g_application_add_main_option_entries(app, conf->main_options);
    g_application_add_main_option_entries(app, version_option);
    GOptionGroup * session_group = g_option_group_new("session",
        "Session options", "Show session options", NULL, NULL);
    g_option_group_add_entries(session_group, conf->session_options);
    GOptionGroup * devices_group = g_option_group_new("device",
        "Device options", "Show device options", NULL, NULL);
    g_option_group_add_entries(devices_group, conf->device_options);
    g_application_add_option_group(app, session_group);
    g_application_add_option_group(app, devices_group);
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

gchar ** client_conf_get_serial_params(ClientConf * conf) {
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

static void read_string(GKeyFile * file, const gchar * group, const gchar * key, gchar ** val) {
    gchar * str_val = g_key_file_get_string(file, group, key, NULL);
    if (str_val) {
        g_debug("Option %s:%s = %s", group, key, str_val);
        *val = str_val;
    }
}

static void read_string_array(GKeyFile * file, const gchar * group, const gchar * key, gchar *** val) {
    gsize size;
    gchar ** str_val = g_key_file_get_string_list(file, group, key, &size, NULL);
    if (str_val) {
        g_debug("Option %s:%s = [%lu](%s, ...)", group, key, size, str_val[0]);
        *val = str_val;
    }
}

static void read_bool(GKeyFile * file, const gchar * group, const gchar * key, gboolean * val) {
    GError * error = NULL;
    gboolean bool_val = g_key_file_get_boolean(file, group, key, &error);
    if (!error) {
        *val = bool_val;
        g_debug("Option %s:%s = %s", group, key, bool_val ? "true" : "false");
    } else g_error_free(error);
}

gchar * get_config_filename() {
    return g_build_filename(
        g_get_user_config_dir(),
        "flexVDI Client",
        "settings.ini",
        NULL
    );
}

static void client_conf_load(ClientConf * conf) {
    GError * error = NULL;
    g_autofree gchar * config_filename = get_config_filename();

    if (!g_key_file_load_from_file(conf->file, config_filename,
            G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning("Error loading settings file: %s", error->message);
        g_error_free(error);
        return;
    }

    struct { const gchar * group; GOptionEntry * options; } groups[] = {
        { "General", conf->main_options },
        { "Session", conf->session_options },
        { "Devices", conf->device_options }
    };

    for (int i = 0; i < G_N_ELEMENTS(groups); ++i) {
        GOptionEntry * option = groups[i].options;
        while (option->long_name != NULL) {
            switch (option->arg) {
            case G_OPTION_ARG_STRING:
                read_string(conf->file, groups[i].group, option->long_name,
                            (gchar **)option->arg_data);
                break;
            case G_OPTION_ARG_NONE:
                read_bool(conf->file, groups[i].group, option->long_name,
                          (gboolean *)option->arg_data);
                break;
            case G_OPTION_ARG_STRING_ARRAY:
                read_string_array(conf->file, groups[i].group, option->long_name,
                                  (gchar ***)option->arg_data);
                break;
            default:;
            }
            ++option;
        }
    }
}

static void write_string(GKeyFile * file, const gchar * group, const gchar * key, gchar * val) {
    if (val)
        g_key_file_set_string(file, group, key, val);
    else if (g_key_file_has_key(file, group, key, NULL))
        g_key_file_remove_key(file, group, key, NULL);
}

static void write_string_array(GKeyFile * file, const gchar * group, const gchar * key, gchar ** val) {
    if (val)
        g_key_file_set_string_list(file, group, key, (const gchar **)val, g_strv_length(val));
    else if (g_key_file_has_key(file, group, key, NULL))
        g_key_file_remove_key(file, group, key, NULL);
}

static void write_bool(GKeyFile * file, const gchar * group, const gchar * key, gboolean val) {
    if (val)
        g_key_file_set_boolean(file, group, key, val);
    else if (g_key_file_has_key(file, group, key, NULL))
        g_key_file_remove_key(file, group, key, NULL);
}

void client_conf_save(ClientConf * conf) {
    GError * error = NULL;
    g_autofree gchar * config_filename = get_config_filename();

    struct { const gchar * group; GOptionEntry * options; } groups[] = {
        { "General", conf->main_options },
        { "Session", conf->session_options },
        { "Devices", conf->device_options }
    };

    for (int i = 0; i < G_N_ELEMENTS(groups); ++i) {
        GOptionEntry * option = groups[i].options;
        while (option->long_name != NULL) {
            switch (option->arg) {
            case G_OPTION_ARG_STRING:
                write_string(conf->file, groups[i].group, option->long_name,
                             *((gchar **)option->arg_data));
                break;
            case G_OPTION_ARG_NONE:
                write_bool(conf->file, groups[i].group, option->long_name,
                           *((gboolean *)option->arg_data));
                break;
            case G_OPTION_ARG_STRING_ARRAY:
                write_string_array(conf->file, groups[i].group, option->long_name,
                                   *((gchar ***)option->arg_data));
                break;
            default:;
            }
            ++option;
        }
    }

    if (!g_key_file_save_to_file(conf->file, config_filename, &error)) {
        g_warning("Error saving settings file: %s", error->message);
        g_error_free(error);
    }
}
