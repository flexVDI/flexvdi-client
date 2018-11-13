#include <stdlib.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include "client-app.h"


typedef struct _LevelForDomain {
    gchar * domain;
    GLogLevelFlags level;
} LevelForDomain;


static GList * level_for_domains = NULL;
static GLogLevelFlags default_level = G_LOG_LEVEL_MESSAGE;
static FILE * log_file = NULL;


static int get_level_for_domain(const gchar * domain) {
    GList * i;
    for (i = level_for_domains; i; i = i->next) {
        LevelForDomain * lfd = (LevelForDomain *)i->data;
        if (g_str_equal(lfd->domain, domain)) return lfd->level;
    }
    return default_level;
}


static void update_level_for_domain(const gchar * domain, GLogLevelFlags level) {
    GList * i;
    for (i = level_for_domains; i; i = i->next) {
        LevelForDomain * lfd = (LevelForDomain *)i->data;
        if (g_str_equal(lfd->domain, domain)) {
            lfd->level = level;
            return;
        }
    }
    LevelForDomain * lfd = g_new0(LevelForDomain, 1);
    lfd->domain = g_strdup(domain);
    lfd->level = level;
    level_for_domains = g_list_prepend(level_for_domains, lfd);
}


GLogWriterOutput log_to_file(GLogLevelFlags log_level, const GLogField * fields,
                             gsize n_fields, gpointer user_data);


static void parse_verbose_levels(const gchar * verbose_str) {
    GLogLevelFlags map[] = {
        G_LOG_LEVEL_ERROR, G_LOG_LEVEL_CRITICAL, G_LOG_LEVEL_WARNING,
        G_LOG_LEVEL_MESSAGE, G_LOG_LEVEL_INFO, G_LOG_LEVEL_DEBUG
    };
    if (verbose_str == NULL) return;

    gchar ** levels = g_strsplit(verbose_str, ",", 0), ** level;
    for (level = levels; *level; ++level) {
        if (**level == '\0') continue;

        gchar ** terms = g_strsplit(*level, ":", 0);
        if (*(terms + 1) == NULL) {
            long int level_num = strtol(*terms, NULL, 10);
            if (level_num >= 0 && level_num <= 5) {
                default_level = map[level_num];
            }
        } else {
            long int level_num = strtol(*(terms + 1), NULL, 10);
            if (level_num >= 0 && level_num <= 5) {
                if (g_str_equal(*terms, "all")) {
                    default_level = map[level_num];
                } else {
                    update_level_for_domain(*terms, map[level_num]);
                }
            }
        }

        g_strfreev(terms);
    }

    g_strfreev(levels);
}


static void setup_logging(const gchar * verbose_str) {
    parse_verbose_levels(verbose_str);
    g_setenv("SPICE_DEBUG", "1", FALSE);

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

    g_log_set_writer_func(log_to_file, NULL, NULL);
}


GLogWriterOutput log_to_file(GLogLevelFlags log_level, const GLogField * fields,
                             gsize n_fields, gpointer user_data) {
    gsize i;
    const gchar * log_domain = NULL, * message = NULL, * log_level_str;

    GLogLevelFlags max_level = default_level;
    if (level_for_domains != NULL) {
        for (i = 0; i < n_fields; ++i) {
            if (g_str_equal(fields[i].key, "GLIB_DOMAIN")) {
                log_domain = (const gchar *)fields[i].value;
                max_level = get_level_for_domain(log_domain);
                break;
            }
        }
    }
    if (log_level > max_level) return G_LOG_WRITER_HANDLED;

    for (i = 0; (log_domain == NULL || message == NULL) && i < n_fields; ++i) {
        if (g_str_equal(fields[i].key, "GLIB_DOMAIN")) {
            log_domain = (const gchar *)fields[i].value;
        } else if (g_str_equal(fields[i].key, "MESSAGE")) {
            message = (const gchar *)fields[i].value;
        }
    }

    switch (log_level) {
        case G_LOG_LEVEL_ERROR: log_level_str = "ERROR"; break;
        case G_LOG_LEVEL_CRITICAL: log_level_str = "CRITICAL"; break;
        case G_LOG_LEVEL_WARNING: log_level_str = "WARNING"; break;
        case G_LOG_LEVEL_MESSAGE: log_level_str = "MESSAGE"; break;
        case G_LOG_LEVEL_INFO: log_level_str = "INFO"; break;
        default: log_level_str = "DEBUG"; break;
    }

    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree gchar * now_str = g_date_time_format(now, "%H:%M:%S");
    fprintf(log_file, "%s.%03d: %s-%s: %s\n", now_str, g_date_time_get_microsecond(now)/1000,
            log_domain, log_level_str, message);

    if (log_level & G_LOG_FLAG_FATAL) G_BREAKPOINT();

    return G_LOG_WRITER_HANDLED;
}


int main (int argc, char * argv[]) {
    /*
     * verbose_levels is a string associating a domain with a log level.
     * It has the format dom1:level1,dom2:level2,...
     * Domain can be any string representing a valid domain, or 'all' for all domains.
     * If omited, 'all' is assumed, so a single number is also a valid verbose string.
     * Levels go from 0 (Error) to 5 (Debug).
     */
    const gchar * verbose_levels = g_getenv("FLEXVDI_LOG_LEVEL");

    // Check the --version and --verbose options as early as possible
    int i;
    for (i = 0; i < argc; ++i) {
        if (g_strcmp0(argv[i], "--version") == 0) {
            printf("flexVDI Client v" VERSION_STRING "\n"
                   "Copyright (C) 2018 Flexible Software Solutions S.L.\n");
            return 0;
        }
        else if (g_strcmp0(argv[i], "--verbose") == 0 || g_strcmp0(argv[i], "-v") == 0) {
            if (i + 1 < argc)
                verbose_levels = argv[i + 1];
        }
    }

    setup_logging(verbose_levels);
    g_message("Starting flexVDI Client v" VERSION_STRING);

    gst_init(&argc, &argv);
    return g_application_run(G_APPLICATION(client_app_new()), argc, argv);
}
