/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#include <gst/gst.h>
#include "configuration.h"

struct _ClientConf {
    GObject parent;
    GOptionEntry * main_options, * session_options, * device_options;
    GKeyFile * file;
    gboolean version;
    // Main options
    gchar * host;
    gchar * port;
    gchar * username;
    gchar * password;
    gchar * terminal_id;
    gchar * uri;
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
    gboolean disable_video_streaming;
    gboolean disable_audio_playback;
    gboolean disable_audio_record;
    gchar * preferred_compression;
    gchar * grab_sequence;
    // Device options
    gchar ** redir_rports;
    gchar ** redir_lports;
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

static void client_conf_init(ClientConf * conf) {
    // Inline initialization of the command-line options groups
    GOptionEntry main_options[] = {
        // { long_name, short_name, flags, arg, arg_data, description, arg_description },
        { "host", 'h', 0, G_OPTION_ARG_STRING, &conf->host,
        "Connection host address", "<hostname or IP>" },
        { "port", 'p', 0, G_OPTION_ARG_STRING, &conf->port,
        "Connection port (default 443)", "<port number>" },
        { "username", 'u', 0, G_OPTION_ARG_STRING, &conf->username,
        "User name", "<user name>" },
        { "password", 'w', 0, G_OPTION_ARG_STRING, &conf->password,
        "Password", "<password>" },
        { "terminal_id", 0, 0, G_OPTION_ARG_STRING, &conf->terminal_id,
        "Use a given Terminal ID instead of calculating it automatically", "<terminal ID>" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };

    GOptionEntry session_options[] = {
        { "desktop", 'd', 0, G_OPTION_ARG_STRING, &conf->desktop,
        "Desktop name to connect to", "<desktop name>" },
        { "proxy-uri", 0, 0, G_OPTION_ARG_STRING, &conf->proxy_uri,
        "Use this URI to connect through a proxy server (default use system settings)", "<uri>" },
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
        { "disable-copy-from-guest", 0, 0, G_OPTION_ARG_NONE, &conf->disable_copy_from_guest,
        "Disable clipboard from guest to client", NULL },
        { "disable-paste-to-guest", 0, 0, G_OPTION_ARG_NONE, &conf->disable_paste_to_guest,
        "Disable clipboard from client to guest", NULL },
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
        { "disable-video-streaming", 0, 0, G_OPTION_ARG_NONE, &conf->disable_video_streaming,
        "Disable video streaming", NULL },
        { "disable-audio-playback", 0, 0, G_OPTION_ARG_NONE, &conf->disable_audio_playback,
        "Disable audio playback from guest", NULL },
        { "disable-audio-record", 0, 0, G_OPTION_ARG_NONE, &conf->disable_audio_record,
        "Disable audio record to guest", NULL },
        { "preferred-compression", 0, 0, G_OPTION_ARG_STRING, &conf->preferred_compression,
        "Preferred image compression algorithm", "<auto-glz,auto-lz,quic,glz,lz,lz4,off>" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };

    GOptionEntry device_options[] = {
        { "redirect-rport", 'R', 0, G_OPTION_ARG_STRING_ARRAY, &conf->redir_rports,
        "Redirect a remote TCP port. Can appear multiple times", "[bind_address:]guest_port:host:host_port" },
        { "redirect-lport", 'L', 0, G_OPTION_ARG_STRING_ARRAY, &conf->redir_lports,
        "Redirect a local TCP port. Can appear multiple times", "[bind_address:]local_port:host:host_port", },
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

    // Dafault values other than 0, NULL or FALSE
    conf->auto_clipboard = TRUE;
    conf->grab_mouse = TRUE;
    conf->grab_sequence = g_strdup("Shift_L+F12");
    conf->resize_guest = TRUE;
    conf->main_options = g_memdup(main_options, sizeof(main_options));
    conf->session_options = g_memdup(session_options, sizeof(session_options));
    conf->device_options = g_memdup(device_options, sizeof(device_options));
    // Load the configuration file
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
    g_free(conf->proxy_uri);
    g_strfreev(conf->serial_params);
    g_strfreev(conf->redir_rports);
    g_strfreev(conf->redir_lports);
    g_free(conf->usb_auto_filter);
    g_free(conf->usb_connect_filter);
    g_free(conf->preferred_compression);
    g_free(conf->terminal_id);
    g_strfreev(conf->printers);
    g_key_file_free(conf->file);
    G_OBJECT_CLASS(client_conf_parent_class)->finalize(obj);
}


ClientConf * client_conf_new(void) {
    return g_object_new(CLIENT_CONF_TYPE, NULL);
}


void client_conf_get_options_from_response(ClientConf * conf, JsonObject * params) {
    if (json_object_has_member(params, "enable_power_actions"))
        conf->disable_power_actions |= !json_object_get_boolean_member(params, "enable_power_actions");
    if (json_object_has_member(params, "enable_usb_redir"))
        conf->disable_usbredir |= !json_object_get_boolean_member(params, "enable_usb_redir");
    if (json_object_has_member(params, "enable_video_streaming"))
        conf->disable_video_streaming |= !json_object_get_boolean_member(params, "enable_video_streaming");
    if (json_object_has_member(params, "enable_audio_playback"))
        conf->disable_audio_playback |= !json_object_get_boolean_member(params, "enable_audio_playback");
    if (json_object_has_member(params, "enable_audio_record"))
        conf->disable_audio_record |= !json_object_get_boolean_member(params, "enable_audio_record");
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
    // The version option is here so that it is not read from the config file
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
    g_application_add_option_group(app, gst_init_get_option_group());
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
    if (conf->redir_rports)
        g_object_set(session, "redirected-remote-ports", conf->redir_rports, NULL);
    if (conf->redir_lports)
        g_object_set(session, "redirected-local-ports", conf->redir_lports, NULL);
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
    g_object_set(session, "enable-audio",
        !conf->disable_audio_playback && !conf->disable_audio_record, NULL);

    SpiceGtkSession * gtk_session = spice_gtk_session_get(session);
    g_object_set(gtk_session,
        "sync-modifiers", TRUE,
        "auto-clipboard", conf->auto_clipboard,
        "auto-usbredir", conf->auto_usbredir,
        "disable-paste-from-guest", conf->disable_copy_from_guest,
        "disable-copy-to-guest", conf->disable_paste_to_guest,
        NULL);
}


void client_conf_set_display_options(ClientConf * conf, SpiceDisplay * display) {
    g_object_set(display,
        "grab-keyboard", TRUE,
        "grab-mouse", conf->grab_mouse,
        "resize-guest", conf->resize_guest,
        "scaling", TRUE,
        "disable-inputs", FALSE,
        NULL);
    SpiceGrabSequence *seq = spice_grab_sequence_new_from_string(conf->grab_sequence);
    spice_display_set_grab_keys(display, seq);
    spice_grab_sequence_free(seq);
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


const gchar * client_conf_get_uri(ClientConf * conf) {
    return conf->uri;
}


const gchar * client_conf_get_proxy_uri(ClientConf * conf) {
    return conf->proxy_uri;
}


gchar * client_conf_get_connection_uri(ClientConf * conf, const gchar * path) {
    if (!conf->host) return NULL;
    if (!conf->port || !g_strcmp0(conf->port, ""))
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
        write_string(conf->file, "General", "terminal_id", conf->terminal_id);
        client_conf_save(conf);
    }
    return conf->terminal_id;
}


void client_conf_set_host(ClientConf * conf, const gchar * host) {
    g_free(conf->host);
    conf->host = g_strdup(host);
    write_string(conf->file, "General", "host", conf->host);
}


void client_conf_set_port(ClientConf * conf, const gchar * port) {
    g_free(conf->port);
    conf->port = (port && port[0]) ? g_strdup(port) : NULL;
    write_string(conf->file, "General", "port", conf->port);
}


void client_conf_set_username(ClientConf * conf, const gchar * username) {
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
    conf->fullscreen = fs;
    write_bool(conf->file, "Session", "fullscreen", conf->fullscreen);
}


void client_conf_share_printer(ClientConf * conf, const gchar * printer, gboolean share) {
    // Insert/remove the printer from the list, but appear only once
    gchar ** sel_printer = conf->printers;
    while (*sel_printer && g_strcmp0(*sel_printer, printer)) ++sel_printer;

    if (share && sel_printer == NULL) {
        int i = g_strv_length(conf->printers);
        gchar ** new_printers = g_malloc_n(i + 1, sizeof(gchar *));
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


/*
 * read_string
 *
 * Read a string value from the key file into a variable, only if it exists
 */
static void read_string(GKeyFile * file, const gchar * group, const gchar * key, gchar ** val) {
    gchar * str_val = g_key_file_get_string(file, group, key, NULL);
    if (str_val) {
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


/*
 * get_config_dir
 *
 * Get the directory where the configuration file is saved.
 */
static gchar * get_config_dir() {
    return g_build_filename(
        g_get_user_config_dir(),
        "flexvdi-client",
        NULL
    );
}


/*
 * get_config_filename
 *
 * Get the path to the configuration file
 */
static gchar * get_config_filename() {
    g_autofree gchar * config_dir = get_config_dir();
    return g_build_filename(
        config_dir,
        "settings.ini",
        NULL
    );
}


/*
 * client_conf_load
 *
 * Load the configuration file
 */
static void client_conf_load(ClientConf * conf) {
    GError * error = NULL;
    g_autofree gchar * config_filename = get_config_filename();
    int i;

    if (!g_key_file_load_from_file(conf->file, config_filename,
            G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning("Error loading settings file: %s", error->message);
        g_error_free(error);
        return;
    }

    // Load options in a section for each option group
    struct { const gchar * group; GOptionEntry * options; } groups[] = {
        { "General", conf->main_options },
        { "Session", conf->session_options },
        { "Devices", conf->device_options }
    };

    for (i = 0; i < G_N_ELEMENTS(groups); ++i) {
        GOptionEntry * option = groups[i].options;
        for (; option->long_name != NULL; ++option) {
            switch (option->arg) {
            case G_OPTION_ARG_STRING:
                read_string(conf->file, groups[i].group, option->long_name,
                            (gchar **)option->arg_data);
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


void client_conf_save(ClientConf * conf) {
    GError * error = NULL;
    g_autofree gchar * config_filename = get_config_filename();
    g_autofree gchar * config_dir = get_config_dir();
#ifdef _WIN32
    g_autofree gchar * tmp_data = g_key_file_to_data(conf->file, NULL, NULL);
    gchar ** lines = g_strsplit(tmp_data, "\n", 0);
    g_autofree gchar * config_data = g_strjoinv("\r\n", lines);
    g_strfreev(lines);
#else
    g_autofree gchar * config_data = g_key_file_to_data(conf->file, NULL, NULL);
#endif

    g_mkdir_with_parents(config_dir, 0755);
    if (!g_file_set_contents(config_filename, config_data, -1, &error)) {
        g_warning("Error saving settings file: %s", error->message);
        g_error_free(error);
    }
}
