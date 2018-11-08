#include <stdlib.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include "client-app.h"


static gchar ** debug_domains = NULL;
static gboolean all_debug = FALSE;
static FILE * log_file = NULL;


static void setup_logging() {
    const gchar * debug_filter = g_getenv("G_MESSAGES_DEBUG");
    debug_domains = g_strsplit(debug_filter ? debug_filter : "", ",", -1);
    gchar ** domain;
    for (domain = debug_domains; *domain != NULL && !all_debug; ++domain)
        all_debug = g_strcmp0(*domain, "all") == 0;

    const gchar * log_file_pattern = g_getenv("FLEXVDI_LOG_FILE");
    if (!log_file_pattern)
        log_file_pattern = "flexvdi-client-%Y-%m-%d.log";
    if (!g_strcmp0(log_file_pattern, "-")) {
        log_file = stderr;
    } else {
        g_autoptr(GDateTime) now = g_date_time_new_now_local();
        g_autofree gchar * file_name = g_date_time_format(now, log_file_pattern);
        g_autofree gchar * log_dir = g_build_filename(g_get_user_data_dir(), "flexvdi-client", NULL);
        g_autofree gchar * file_path = g_build_filename(log_dir, file_name, NULL);
        g_mkdir_with_parents(log_dir, 0700);
        FILE * file = fopen(file_path, "a");
        log_file = file ? file : stderr;
    }
    setvbuf(log_file, NULL, _IOLBF, 0);
}


static gboolean is_debug_domain(const gchar * domain_name) {
    if (all_debug) return TRUE;
    if (!domain_name) return FALSE;
    gchar ** domain;
    for (domain = debug_domains; *domain != NULL; ++domain)
        if (g_strcmp0(*domain, domain_name) == 0) return TRUE;
    return FALSE;
}


static void log_to_file(const gchar * log_domain, GLogLevelFlags log_level,
                        const gchar * message, gpointer user_data) {
    const gchar * log_level_str = NULL;
    if (log_level & G_LOG_LEVEL_DEBUG) {
        log_level_str = "DEBUG";
        if (!is_debug_domain(log_domain)) return;
    }
    else if (log_level & G_LOG_LEVEL_ERROR) log_level_str = "ERROR";
    else if (log_level & G_LOG_LEVEL_CRITICAL) log_level_str = "CRITICAL";
    else if (log_level & G_LOG_LEVEL_WARNING) log_level_str = "WARNING";
    else if (log_level & G_LOG_LEVEL_MESSAGE) log_level_str = "MESSAGE";
    else if (log_level & G_LOG_LEVEL_INFO) log_level_str = "INFO";

    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree gchar * now_str = g_date_time_format(now, "%H:%M:%S");
    fprintf(log_file, "%s.%03d: %s-%s: %s\n", now_str, g_date_time_get_microsecond(now)/1000,
            log_domain, log_level_str, message);

    if (log_level & G_LOG_FLAG_FATAL) abort();
}


int main (int argc, char * argv[]) {
    // Check the --version option as early as possible
    int i;
    for (i = 0; i < argc; ++i) {
        if (g_strcmp0(argv[i], "--version") == 0) {
            printf("flexVDI Client v" VERSION_STRING "\n"
                   "Copyright (C) 2018 Flexible Software Solutions S.L.\n");
            return 0;
        }
    }

    setup_logging();
    g_log_set_default_handler(log_to_file, NULL);
    g_info("Starting flexVDI Client v" VERSION_STRING);

    gst_init(&argc, &argv);
    return g_application_run(G_APPLICATION(client_app_new()), argc, argv);
}
