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

#include <stdlib.h>
#include "configuration.h"
#include "client-log.h"

struct _ClientConf {
    GObject parent;
    gchar ** arguments;
    GHashTable * cmdline_options;
    GOptionEntry * main_options, * session_options, * device_options, * all_options;
    GKeyFile * file;
    gchar * file_name;
    gboolean had_file;
    SoupSession * soup;
    // Main options
    gchar * host;
    gchar * port;
    gchar * username;
    gchar * password;
    gchar * terminal_id;
    gchar * uri;
    gboolean kiosk_mode;
    // Session options
    gchar * desktop;
    gchar * proxy_uri;
    gboolean fullscreen;
    gint inactivity_timeout;
    gboolean disable_printing;
    gboolean auto_clipboard;
    gboolean auto_usbredir;
    gboolean disable_copy_from_guest;
    gboolean disable_paste_to_guest;
    gboolean grab_mouse;
    gboolean resize_guest;
    gboolean disable_power_actions;
    gboolean disable_usbredir;
    gchar * preferred_compression;
    gchar * grab_sequence;
    gchar * shared_folder;
    gboolean shared_folder_ro;
    WindowEdge toolbar_edge;
    // Device options
    gchar * usb_auto_filter;
    gchar * usb_connect_filter;
    gchar ** serial_params;
    gchar ** printers;
};

G_DEFINE_TYPE(ClientConf, client_conf, G_TYPE_OBJECT);


static void client_conf_finalize(GObject * obj);

static void client_conf_class_init(ClientConfClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->finalize = client_conf_finalize;
}


static void client_conf_load(ClientConf * conf);
static gboolean set_proxy_uri(const gchar * option_name, const gchar * value, gpointer data, GError ** error);
static gboolean set_toolbar_edge(const gchar * option_name, const gchar * value, gpointer data, GError ** error);
static gboolean set_log_level(const gchar * option_name, const gchar * value, gpointer data, GError ** error);
static gboolean add_option_to_table(const gchar * option_name, const gchar * value, gpointer data, GError ** error);

static void client_conf_init(ClientConf * conf) {
    // Inline initialization of the command-line options groups
    GOptionEntry main_options[] = {
        // { long_name, short_name, flags, arg, arg_data, description, arg_description },
        { "host", 'h', 0, G_OPTION_ARG_STRING, &conf->host,
        "Connection host address", "<hostname or IP>" },
        { "port", 'p', 0, G_OPTION_ARG_STRING, &conf->port,
        "Connection port (default 443)", "<port number>" },
        { "proxy-uri", 0, 0, G_OPTION_ARG_CALLBACK, set_proxy_uri,
        "Use this URI to connect through a proxy server (default use system settings)", "<uri>" },
        { "username", 'u', 0, G_OPTION_ARG_STRING, &conf->username,
        "User name", "<user name>" },
        { "password", 'w', 0, G_OPTION_ARG_STRING, &conf->password,
        "Password", "<password>" },
        { "desktop", 'd', 0, G_OPTION_ARG_STRING, &conf->desktop,
        "Desktop name to connect to", "<desktop name>" },
        { "terminal-id", 0, 0, G_OPTION_ARG_STRING, &conf->terminal_id,
        "Use a given Terminal ID instead of calculating it automatically", "<terminal ID>" },
        { "kiosk", 0, 0, G_OPTION_ARG_NONE, &conf->kiosk_mode,
        "Enable kiosk mode", NULL },
        { "log-level", 'v', 0, G_OPTION_ARG_CALLBACK, set_log_level,
        "Log level for each domain, or for all messages if domain is ommited. 0 = ERROR, 5 = DEBUG", "<[domain:]level,...>" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };
    gsize num_main_options = G_N_ELEMENTS(main_options) - 1;

    GOptionEntry session_options[] = {
        { "fullscreen", 'f', 0, G_OPTION_ARG_NONE, &conf->fullscreen,
        "Show desktop in fullscreen mode", NULL },
        { "no-fullscreen", 0, G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_REVERSE,
        G_OPTION_ARG_NONE, &conf->fullscreen, "", NULL },
        { "inactivity-timeout", 0, 0, G_OPTION_ARG_INT, &conf->inactivity_timeout,
        "Close the client after a certain time of inactivity", "<seconds>" },
        { "flexvdi-disable-printing", 0, 0, G_OPTION_ARG_NONE, &conf->disable_printing,
        "Disable printing support", NULL },
        { "auto-clipboard", 0, 0, G_OPTION_ARG_NONE, &conf->auto_clipboard,
        "Automatically share clipboard between guest and client", NULL },
        { "no-auto-clipboard", 0, G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_REVERSE,
        G_OPTION_ARG_NONE, &conf->auto_clipboard, "", NULL },
        { "auto-usbredir", 0, 0, G_OPTION_ARG_NONE, &conf->auto_usbredir,
        "Automatically redirect newly connected USB devices", NULL },
        { "no-auto-usbredir", 0, G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_REVERSE,
        G_OPTION_ARG_NONE, &conf->auto_usbredir, "", NULL },
        { "grab-mouse", 0, 0, G_OPTION_ARG_NONE, &conf->grab_mouse,
        "Grab mouse in server mode", NULL },
        { "no-grab-mouse", 0, G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_REVERSE,
        G_OPTION_ARG_NONE, &conf->grab_mouse, "", NULL },
        { "grab-sequence", 0, 0, G_OPTION_ARG_STRING, &conf->grab_sequence,
        "Key sequence to release mouse grab (default Shift_L+F12)", NULL },
        { "resize-guest", 0, 0, G_OPTION_ARG_NONE, &conf->resize_guest,
        "Resize guest display to match window size", NULL },
        { "no-resize-guest", 0, G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_REVERSE,
        G_OPTION_ARG_NONE, &conf->resize_guest, "", NULL },
        { "disable-power-actions", 0, 0, G_OPTION_ARG_NONE, &conf->disable_power_actions,
        "Disable reset/poweroff guest actions", NULL },
        { "disable-usbredir", 0, 0, G_OPTION_ARG_NONE, &conf->disable_usbredir,
        "Disable USB device redirection", NULL },
        { "preferred-compression", 0, 0, G_OPTION_ARG_STRING, &conf->preferred_compression,
        "Preferred image compression algorithm", "<auto-glz,auto-lz,quic,glz,lz,lz4,off>" },
        { "shared-folder", 0, 0, G_OPTION_ARG_STRING, &conf->shared_folder,
        "Shared directory with the guest", NULL },
        { "shared-folder-ro", 0, 0, G_OPTION_ARG_NONE, &conf->shared_folder_ro,
        "Shared directory with the guest is readonly", NULL },
        { "toolbar-edge", 0, 0, G_OPTION_ARG_CALLBACK, set_toolbar_edge,
        "Window edge where toolbar is shown (default top)", "<top,bottom,left,right>" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };
    gsize num_session_options = G_N_ELEMENTS(session_options) - 1;

    GOptionEntry device_options[] = {
        { "usbredir-auto-redirect-filter", 0, 0, G_OPTION_ARG_STRING, &conf->usb_auto_filter,
          "Filter selecting USB devices to be auto-redirected when plugged in", "<filter-string>" },
        { "usbredir-redirect-on-connect", 0, 0, G_OPTION_ARG_STRING, &conf->usb_connect_filter,
          "Filter selecting USB devices to redirect on connect", "<filter-string>" },
        { "flexvdi-serial-port", 0, 0, G_OPTION_ARG_STRING_ARRAY, &conf->serial_params,
        "Add serial port redirection. Can appear multiple times. "
        "Example: /dev/ttyS0,9600,8N1", "<device,speed,mode>" },
        { "share-printer", 'P', 0, G_OPTION_ARG_STRING_ARRAY, &conf->printers,
        "Share a client's printer with the virtual desktop. Can appear multiple times",
        "<printer_name>" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };
    gsize num_device_options = G_N_ELEMENTS(device_options) - 1;

    // Dafault values other than 0, NULL or FALSE
    conf->auto_clipboard = TRUE;
    conf->grab_mouse = TRUE;
    conf->grab_sequence = g_strdup("Shift_L+F12");
    conf->resize_guest = TRUE;
    conf->main_options = g_memdup(main_options, sizeof(main_options));
    conf->session_options = g_memdup(session_options, sizeof(session_options));
    conf->device_options = g_memdup(device_options, sizeof(device_options));
    conf->all_options = g_new0(GOptionEntry, num_main_options + num_session_options + num_device_options + 1);
    GOptionEntry * o = conf->all_options;
    memcpy(o, main_options, sizeof(main_options));
    memcpy(o + num_main_options, session_options, sizeof(session_options));
    memcpy(o + num_main_options + num_session_options, device_options, sizeof(device_options));
    conf->printers = g_strsplit("", ".", 0);
    conf->toolbar_edge =
#ifdef __APPLE__
        WINDOW_EDGE_DOWN;
#else
        WINDOW_EDGE_UP;
#endif
    conf->cmdline_options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    conf->file = g_key_file_new();
    conf->file_name = g_build_filename(
        g_get_user_config_dir(),
        "flexvdi-client",
        "settings.ini",
        NULL
    );
    // FIXME: Disable checking server certificate
    conf->soup = soup_session_new_with_options(
        "ssl-strict", FALSE,
        "timeout", 5,
        NULL);
}


static void client_conf_finalize(GObject * obj) {
    ClientConf * conf = CLIENT_CONF(obj);
    g_free(conf->main_options);
    g_free(conf->session_options);
    g_free(conf->device_options);
    g_free(conf->all_options);
    g_free(conf->host);
    g_free(conf->port);
    g_free(conf->username);
    g_free(conf->password);
    g_free(conf->desktop);
    g_free(conf->proxy_uri);
    g_strfreev(conf->serial_params);
    g_free(conf->usb_auto_filter);
    g_free(conf->usb_connect_filter);
    g_free(conf->preferred_compression);
    g_free(conf->terminal_id);
    g_free(conf->shared_folder);
    g_strfreev(conf->printers);
    g_key_file_free(conf->file);
    g_hash_table_unref(conf->cmdline_options);
    g_strfreev(conf->arguments);
    G_OBJECT_CLASS(client_conf_parent_class)->finalize(obj);
}


ClientConf * client_conf_new(void) {
    return g_object_new(CLIENT_CONF_TYPE, NULL);
}


static gboolean add_option_to_table(const gchar * option_name, const gchar * value, gpointer data, GError ** error) {
    ClientConf * conf = CLIENT_CONF(data);

    if (g_str_has_prefix(option_name, "--no-") && !value) {
        g_hash_table_insert(conf->cmdline_options, g_strdup(&option_name[5]), g_strdup("false"));
    } else {
        if (!value) value = "true";
        if (g_str_has_prefix(option_name, "--")) {
            g_hash_table_insert(conf->cmdline_options, g_strdup(&option_name[2]), g_strdup(value));
        } else {
            // Single letter, look for long name
            GOptionEntry * option = conf->all_options;
            for(; option->long_name; ++option) {
                if (option->short_name == option_name[1]) {
                    g_hash_table_insert(conf->cmdline_options, g_strdup(option->long_name), g_strdup(value));
                    break;
                }
            }
        }
    }

    return TRUE;
}


void client_conf_set_original_arguments(ClientConf * conf, gchar ** arguments) {
    conf->arguments = g_strdupv(arguments);
}


/*
 * Handle command-line options once GLib has gone over them. Return -1 on success.
 */
static gint client_conf_handle_options(GApplication * gapp, GVariantDict * opts, gpointer user_data) {
    ClientConf * conf = CLIENT_CONF(user_data);

    if (g_variant_dict_contains(opts, "version")) {
        g_print("flexVDI Client v" VERSION_STRING "\n"
                "Copyright (C) 2018 Flexible Software Solutions S.L.\n");
        return 0;
    }

    // Command line parsing is finished, program starts now
    g_message("Starting flexVDI Client v" VERSION_STRING);

    // Parse again the conf->arguments copy of the command line arguments,
    // just to make a list of the options that must not be loaded from file
    GOptionEntry * o = conf->all_options;
    for (; o->long_name; ++o) {
        o->flags = G_OPTION_FLAG_OPTIONAL_ARG;
        o->arg = G_OPTION_ARG_CALLBACK;
        o->arg_data = add_option_to_table;
    }
    g_autoptr(GOptionContext) ctx = g_option_context_new(NULL);
    g_autoptr(GOptionGroup) options = g_option_group_new("", "", "", conf, NULL);
    g_option_group_add_entries(options, conf->all_options);
    g_option_context_add_group(ctx, options);
    g_option_context_set_help_enabled(ctx, FALSE);
    g_option_context_set_ignore_unknown_options(ctx, TRUE);
    g_option_context_parse_strv(ctx, &conf->arguments, NULL);

    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, conf->cmdline_options);
    while (g_hash_table_iter_next(&iter, &key, &value))
        g_debug("Command line option '%s' = '%s'", (gchar *)key, (gchar *)value);

    client_conf_load(conf);

    return -1;
}


void client_conf_get_options_from_response(ClientConf * conf, JsonObject * params) {
    if (json_object_has_member(params, "enable_power_actions"))
        conf->disable_power_actions |= !json_object_get_boolean_member(params, "enable_power_actions");
    if (json_object_has_member(params, "enable_usb_redir"))
        conf->disable_usbredir |= !json_object_get_boolean_member(params, "enable_usb_redir");
    if (json_object_has_member(params, "enable_copy_paste_g2h"))
        conf->disable_copy_from_guest |= !json_object_get_boolean_member(params, "enable_copy_paste_g2h");
    if (json_object_has_member(params, "enable_copy_paste_h2g"))
        conf->disable_paste_to_guest |= !json_object_get_boolean_member(params, "enable_copy_paste_h2g");
    if (json_object_has_member(params, "enable_printing"))
        conf->disable_printing |= !json_object_get_boolean_member(params, "enable_printing");
    if (json_object_has_member(params, "inactivity_timeout")) {
        int timeout_from_manager = json_object_get_int_member(params, "inactivity_timeout");
        if (conf->inactivity_timeout == 0 ||
            (timeout_from_manager > 0 && timeout_from_manager < conf->inactivity_timeout))
            conf->inactivity_timeout = timeout_from_manager;
    }
}


void client_conf_set_application_options(ClientConf * conf, GApplication * app) {
    // Main options not to be read from the config file
    GOptionEntry non_file_options[] = {
        { "version", 'V', 0, G_OPTION_ARG_NONE, NULL,
        "Show version and exit", NULL },
        { "config-file", 'c', 0, G_OPTION_ARG_STRING, &conf->file_name,
        "Alternative configuration file name", "<file name>" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };
    g_application_add_main_option_entries(app, conf->main_options);
    g_application_add_main_option_entries(app, non_file_options);
    GOptionGroup * session_group = g_option_group_new("session",
        "Session options", "Show session options", conf, NULL);
    g_option_group_add_entries(session_group, conf->session_options);
    GOptionGroup * devices_group = g_option_group_new("device",
        "Device options", "Show device options", conf, NULL);
    g_option_group_add_entries(devices_group, conf->device_options);
    g_application_add_option_group(app, session_group);
    g_application_add_option_group(app, devices_group);
    g_signal_connect(app, "handle-local-options",
        G_CALLBACK(client_conf_handle_options), conf);
}


gboolean client_conf_had_file(ClientConf * conf) {
    return conf->had_file;
}


/*
 * parse_preferred_compression
 *
 * Parse a preferred compression value
 */
static void parse_preferred_compression(SpiceSession * session, const gchar * value) {
    int preferred_compression = SPICE_IMAGE_COMPRESSION_INVALID;
    if (!g_strcmp0(value, "auto-glz")) {
        preferred_compression = SPICE_IMAGE_COMPRESSION_AUTO_GLZ;
    } else if (!g_strcmp0(value, "auto-lz")) {
        preferred_compression = SPICE_IMAGE_COMPRESSION_AUTO_LZ;
    } else if (!g_strcmp0(value, "quic")) {
        preferred_compression = SPICE_IMAGE_COMPRESSION_QUIC;
    } else if (!g_strcmp0(value, "glz")) {
        preferred_compression = SPICE_IMAGE_COMPRESSION_GLZ;
    } else if (!g_strcmp0(value, "lz")) {
        preferred_compression = SPICE_IMAGE_COMPRESSION_LZ;
    } else if (!g_strcmp0(value, "lz4")) {
        preferred_compression = SPICE_IMAGE_COMPRESSION_LZ4;
    } else if (!g_strcmp0(value, "off")) {
        preferred_compression = SPICE_IMAGE_COMPRESSION_OFF;
    } else {
        g_warning("Image compression algorithm %s not supported", value);
        return;
    }
    g_object_set(session, "preferred-compression", preferred_compression, NULL);
}


void client_conf_set_session_options(ClientConf * conf, SpiceSession * session) {
    if (conf->proxy_uri && conf->proxy_uri[0])
        g_object_set(session, "proxy", conf->proxy_uri, NULL);
    g_object_set(session, "enable-usbredir", !conf->disable_usbredir, NULL);
    if (!conf->disable_usbredir) {
        SpiceUsbDeviceManager * mgr = spice_usb_device_manager_get(session, NULL);
        if (mgr) {
            if (conf->usb_auto_filter)
                g_object_set(mgr, "auto-connect-filter", conf->usb_auto_filter, NULL);
            if (conf->usb_connect_filter)
                g_object_set(mgr, "redirect-on-connect", conf->usb_connect_filter, NULL);
        } else {
            g_warning("USB redirection support not available");
        }
    }
    if (conf->preferred_compression)
        parse_preferred_compression(session, conf->preferred_compression);
    g_object_set(session, "shared-dir", conf->shared_folder, NULL);
    g_object_set(session, "share-dir-ro", conf->shared_folder_ro, NULL);
}


void client_conf_set_gtk_session_options(ClientConf * conf, GObject * gtk_session) {
    g_object_set(gtk_session,
        "sync-modifiers", TRUE,
        "auto-clipboard", conf->auto_clipboard,
        "auto-usbredir", conf->auto_usbredir,
        NULL);
}


void client_conf_set_display_options(ClientConf * conf, GObject * display) {
    g_object_set(display,
        "grab-keyboard", TRUE,
        "grab-mouse", conf->grab_mouse,
        "resize-guest", conf->resize_guest,
        "scaling", TRUE,
        "disable-inputs", FALSE,
        NULL);
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


const gchar * client_conf_get_uri(ClientConf * conf) {
    return conf->uri;
}


const gchar * client_conf_get_proxy_uri(ClientConf * conf) {
    return conf->proxy_uri;
}


gboolean client_conf_get_kiosk_mode(ClientConf * conf) {
    return conf->kiosk_mode;
}


gchar * client_conf_get_connection_uri(ClientConf * conf, const gchar * path) {
    if (!conf->host) return NULL;
    if (!conf->port || !g_strcmp0(conf->port, ""))
        return g_strdup_printf("https://%s%s", conf->host, path);
    else
        return g_strdup_printf("https://%s:%s%s", conf->host, conf->port, path);
}


gboolean client_conf_get_fullscreen(ClientConf * conf) {
    return conf->fullscreen || conf->kiosk_mode;
}


gchar ** client_conf_get_serial_params(ClientConf * conf) {
    return conf->serial_params;
}


gboolean client_conf_get_disable_printing(ClientConf * conf) {
    return conf->disable_printing;
}


gboolean client_conf_get_disable_copy_from_guest(ClientConf * conf) {
    return conf->disable_copy_from_guest;
}


gboolean client_conf_get_disable_paste_to_guest(ClientConf * conf) {
    return conf->disable_paste_to_guest;
}


gboolean client_conf_get_disable_power_actions(ClientConf * conf) {
    return conf->disable_power_actions;
}


gboolean client_conf_is_printer_shared(ClientConf * conf, const gchar * printer) {
    return conf->printers && g_strv_contains((const gchar * const *)conf->printers, printer);
}


gchar * client_conf_get_grab_sequence(ClientConf * conf) {
    return conf->grab_sequence;
}


gint client_conf_get_inactivity_timeout(ClientConf * conf) {
    return conf->inactivity_timeout;
}


gboolean client_conf_get_auto_clipboard(ClientConf * conf) {
    return conf->auto_clipboard;
}


SoupSession * client_conf_get_soup_session(ClientConf * conf) {
    return conf->soup;
}


WindowEdge client_conf_get_toolbar_edge(ClientConf * conf) {
    return conf->toolbar_edge;
}


/*
 * write_string
 *
 * Write a string value to the key file if it is not NULL, otherwise remove its key
 */
static void write_string(GKeyFile * file, const gchar * group, const gchar * key, gchar * val) {
    if (val)
        g_key_file_set_string(file, group, key, val);
    else g_key_file_remove_key(file, group, key, NULL);
}


/*
 * write_string_array
 *
 * Write a string array value to the key file if it is not NULL, otherwise remove its key
 */

static void write_string_array(GKeyFile * file, const gchar * group, const gchar * key, gchar ** val) {
    if (val)
        g_key_file_set_string_list(file, group, key, (const gchar **)val, g_strv_length(val));
    else g_key_file_remove_key(file, group, key, NULL);
}


/*
 * write_bool
 *
 * Write a boolean value to the key file if it is not FALSE, otherwise remove its key
 */

static void write_bool(GKeyFile * file, const gchar * group, const gchar * key, gboolean val) {
    if (val)
        g_key_file_set_boolean(file, group, key, val);
    else g_key_file_remove_key(file, group, key, NULL);
}


/*
 * write_int
 *
 * Write an integer value to the key file if it is not 0, otherwise remove its key
 */

static void write_int(GKeyFile * file, const gchar * group, const gchar * key, gint val) {
    if (val)
        g_key_file_set_integer(file, group, key, val);
    else g_key_file_remove_key(file, group, key, NULL);
}


/*
 * discover_terminal_id
 *
 * Obtain the terminal ID from the system. Implemented in terminalid-[platform].c
 */
gchar * discover_terminal_id();

const gchar * client_conf_get_terminal_id(ClientConf * conf) {
    if (!conf->terminal_id) {
        conf->terminal_id = discover_terminal_id();
        if (conf->terminal_id[0] == '\0') {
            // TODO: Random terminal id
        }
        write_string(conf->file, "General", "terminal-id", conf->terminal_id);
        client_conf_save(conf);
    }
    return conf->terminal_id;
}


void client_conf_set_host(ClientConf * conf, const gchar * host) {
    if (conf->kiosk_mode) return;

    g_free(conf->host);
    conf->host = g_strdup(host);
    write_string(conf->file, "General", "host", conf->host);
}


void client_conf_set_port(ClientConf * conf, const gchar * port) {
    if (conf->kiosk_mode) return;

    g_free(conf->port);
    conf->port = (port && port[0]) ? g_strdup(port) : NULL;
    write_string(conf->file, "General", "port", conf->port);
}


void client_conf_set_username(ClientConf * conf, const gchar * username) {
    if (conf->kiosk_mode) return;
    if (!g_strcmp0(conf->username, username)) return;

    g_free(conf->username);
    conf->username = g_strdup(username);
    write_string(conf->file, "General", "username", conf->username);
}


void client_conf_set_uri(ClientConf * conf, const gchar * uri) {
    g_free(conf->uri);
    conf->uri = g_strdup(uri);
    // Parse flexvdi:// URIs
}


void client_conf_set_fullscreen(ClientConf * conf, gboolean fs) {
    if (conf->kiosk_mode) return;
    if (fs == conf->fullscreen) return;

    conf->fullscreen = fs;
    write_bool(conf->file, "Session", "fullscreen", conf->fullscreen);
}


static gboolean set_proxy_uri(const gchar * option_name, const gchar * value, gpointer data, GError ** error) {
    ClientConf * conf = CLIENT_CONF(data);
    g_free(conf->proxy_uri);
    conf->proxy_uri = value ? g_strdup(value) : NULL;

    if (conf->proxy_uri && conf->proxy_uri[0]) {
        SoupURI * uri = soup_uri_new(conf->proxy_uri);
        if (uri) {
            g_object_set(conf->soup, "proxy-uri", uri, NULL);
        } else {
            g_warning("Invalid proxy uri %s", conf->proxy_uri);
        }
    } else {
        g_object_set(conf->soup, "proxy-uri", NULL, NULL);
    }

    return TRUE;
}


void client_conf_set_proxy_uri(ClientConf * conf, const gchar * proxy_uri) {
    if (conf->kiosk_mode) return;
    if (!g_strcmp0(conf->proxy_uri, proxy_uri)) return;

    set_proxy_uri("", proxy_uri, conf, NULL);
    write_string(conf->file, "General", "proxy-uri", conf->proxy_uri);
}


static gboolean set_toolbar_edge(const gchar * option_name, const gchar * value,
                                 gpointer data, GError ** error) {
    ClientConf * conf = CLIENT_CONF(data);

    g_autofree gchar * edge = g_strstrip(g_strdup(value));
    if (g_str_equal(edge, "top"))
        conf->toolbar_edge = WINDOW_EDGE_UP;
    else if (g_str_equal(edge, "bottom"))
        conf->toolbar_edge = WINDOW_EDGE_DOWN;
    else if (g_str_equal(edge, "left"))
        conf->toolbar_edge = WINDOW_EDGE_LEFT;
    else if (g_str_equal(edge, "right"))
        conf->toolbar_edge = WINDOW_EDGE_RIGHT;
    else {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                    "Unknown window edge %s", edge);
        return FALSE;
    }

    return TRUE;
}


static gboolean set_log_level(const gchar * option_name, const gchar * value, gpointer data, GError ** error) {
    client_log_set_log_levels(value);
    return TRUE;
}


void client_conf_share_printer(ClientConf * conf, const gchar * printer, gboolean share) {
    // Insert/remove the printer from the list, but appear only once
    gchar ** sel_printer = conf->printers;
    while (*sel_printer && g_strcmp0(*sel_printer, printer)) ++sel_printer;

    if (share && sel_printer == NULL) {
        int i = g_strv_length(conf->printers);
        gchar ** new_printers = g_malloc_n(i + 2, sizeof(gchar *));
        new_printers[i + 1] = NULL;
        new_printers[i--] = g_strdup(printer);
        while (i >= 0) {
            new_printers[i] = g_strdup(conf->printers[i]);
            --i;
        }
        g_strfreev(conf->printers);
        conf->printers = new_printers;
    } else if (!share && sel_printer != NULL) {
        g_free(*sel_printer);
        while (*sel_printer != NULL)
            *sel_printer = *(sel_printer + 1);
    }
}


void client_conf_set_window_size(ClientConf * conf, gint id, int width,
                                 int height, gboolean maximized, int monitor) {
    if (conf->kiosk_mode) return;

    g_autofree gchar * encoded_size = g_strdup_printf("%d,%d,%s,%d",
        width, height, maximized ? "true" : "false", monitor);
    g_autofree gchar * id_str = g_strdup_printf("%d", id);
    write_string(conf->file, "Layout", id_str, encoded_size);
}


/*
 * read_string
 *
 * Read a string value from the key file into a variable, only if it exists
 */
static void read_string(GKeyFile * file, const gchar * group, const gchar * key, gchar ** val) {
    gchar * str_val = g_key_file_get_string(file, group, key, NULL);
    if (str_val) {
        if (!g_str_equal(key, "password"))
            g_debug("Option %s:%s = %s", group, key, str_val);
        *val = str_val;
    }
}


/*
 * read_string_array
 *
 * Read a string array value from the key file into a variable, only if it exists.
 * String arrays are written in the configuration file as a ;-separated list
 */
static void read_string_array(GKeyFile * file, const gchar * group, const gchar * key, gchar *** val) {
    gsize size;
    gchar ** str_val = g_key_file_get_string_list(file, group, key, &size, NULL);
    if (str_val) {
        g_debug("Option %s:%s = [%lu](%s, ...)", group, key, size, str_val[0]);
        *val = str_val;
    }
}


/*
 * read_bool
 *
 * Read a boolean value from the key file into a variable, only if it exists
 */
static void read_bool(GKeyFile * file, const gchar * group, const gchar * key, gboolean * val) {
    GError * error = NULL;
    gboolean bool_val = g_key_file_get_boolean(file, group, key, &error);
    if (!error) {
        *val = bool_val;
        g_debug("Option %s:%s = %s", group, key, bool_val ? "true" : "false");
    } else g_error_free(error);
}


/*
 * read_int
 *
 * Read an integer value from the key file into a variable, only if it exists
 */
static void read_int(GKeyFile * file, const gchar * group, const gchar * key, gint * val) {
    GError * error = NULL;
    gint int_val = g_key_file_get_integer(file, group, key, &error);
    if (!error) {
        *val = int_val;
        g_debug("Option %s:%s = %d", group, key, int_val);
    } else g_error_free(error);
}


gboolean client_conf_get_window_size(ClientConf * conf, gint id,
    int * width, int * height, gboolean * maximized, int * monitor) {
    g_autofree gchar * encoded_size = NULL;
    g_autofree gchar * id_str = g_strdup_printf("%d", id);
    read_string(conf->file, "Layout", id_str, &encoded_size);
    if (encoded_size != NULL) {
        gchar ** size_terms = g_strsplit(encoded_size, ",", 0);
        if (g_strv_length(size_terms) >= 4) {
            *width = strtol(size_terms[0], NULL, 10);
            *height = strtol(size_terms[1], NULL, 10);
            *maximized = !g_ascii_strncasecmp(size_terms[2], "true", 4);
            *monitor = strtol(size_terms[3], NULL, 10);
        }
        g_strfreev(size_terms);
        return TRUE;
    } else return FALSE;
}


static void try_migrate_legacy_config_file(const gchar * new_file_name);

/*
 * client_conf_load
 *
 * Load the configuration file
 */
static void client_conf_load(ClientConf * conf) {
    GError * error = NULL;
    int i;
    gchar * cb_arg;

    if (!g_file_test(conf->file_name, G_FILE_TEST_EXISTS))
        try_migrate_legacy_config_file(conf->file_name);

    if (!g_key_file_load_from_file(conf->file, conf->file_name,
            G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning("Error loading settings file: %s", error->message);
        g_error_free(error);
        return;
    }
    conf->had_file = TRUE;

    // Load options in a section for each option group
    struct { const gchar * group; GOptionEntry * options; } groups[] = {
        { "General", conf->main_options },
        { "Session", conf->session_options },
        { "Devices", conf->device_options }
    };

    for (i = 0; i < G_N_ELEMENTS(groups); ++i) {
        GOptionEntry * option = groups[i].options;
        for (; option->long_name != NULL; ++option) {
            // Skip options provided in the command line
            if (g_hash_table_contains(conf->cmdline_options, option->long_name)) continue;
            switch (option->arg) {
            case G_OPTION_ARG_STRING:
                read_string(conf->file, groups[i].group, option->long_name,
                            (gchar **)option->arg_data);
                break;
            case G_OPTION_ARG_CALLBACK:
                cb_arg = NULL;
                read_string(conf->file, groups[i].group, option->long_name, &cb_arg);
                if (cb_arg) {
                    error = NULL;
                    ((GOptionArgFunc)option->arg_data)("", cb_arg, conf, &error);
                    if (error) {
                        g_warning("Error reading option %s: %s", option->long_name, error->message);
                        g_error_free(error);
                    }
                    g_free(cb_arg);
                }
                break;
            case G_OPTION_ARG_INT:
                read_int(conf->file, groups[i].group, option->long_name,
                         (gint *)option->arg_data);
                break;
            case G_OPTION_ARG_NONE:
                if (g_str_has_prefix(option->long_name, "no-")) continue;
                read_bool(conf->file, groups[i].group, option->long_name,
                          (gboolean *)option->arg_data);
                break;
            case G_OPTION_ARG_STRING_ARRAY:
                read_string_array(conf->file, groups[i].group, option->long_name,
                                  (gchar ***)option->arg_data);
                break;
            default:;
            }
        }
    }
}


static void save_config_file(GKeyFile * file, const gchar * file_name, GError ** error) {
    g_autofree gchar * config_dir = g_path_get_dirname(file_name);
#ifdef _WIN32
    g_autofree gchar * tmp_data = g_key_file_to_data(file, NULL, NULL);
    gchar ** lines = g_strsplit(tmp_data, "\n", 0);
    g_autofree gchar * config_data = g_strjoinv("\r\n", lines);
    g_strfreev(lines);
#else
    g_autofree gchar * config_data = g_key_file_to_data(file, NULL, NULL);
#endif

    g_mkdir_with_parents(config_dir, 0755);
    g_file_set_contents(file_name, config_data, -1, error);
}


void client_conf_save(ClientConf * conf) {
    GError * error = NULL;
    save_config_file(conf->file, conf->file_name, &error);
    if (error) {
        g_warning("Error saving settings file: %s", error->message);
        g_error_free(error);
    }
}


gboolean load_legacy_config_file(GKeyFile * new_file);

static void try_migrate_legacy_config_file(const gchar * new_file_name) {
    g_autoptr(GKeyFile) new_file = g_key_file_new();
    if (load_legacy_config_file(new_file)) {
        GError * error = NULL;
        save_config_file(new_file, new_file_name, &error);
        if (error) {
            g_warning("Error migrating legacy settings file: %s", error->message);
            g_error_free(error);
        }
    }
}
