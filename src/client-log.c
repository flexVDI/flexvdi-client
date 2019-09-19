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

#include <stdio.h>
#include <stdlib.h>
#include <spice-client.h>
#ifdef ANDROID
#include <android/log.h>
#endif
#include "client-log.h"


typedef struct _LevelForDomain {
    gchar * domain;
    GLogLevelFlags level;
} LevelForDomain;


static GList * level_for_domains = NULL;
static GLogLevelFlags default_level = G_LOG_LEVEL_MESSAGE;
static GLogLevelFlags fatal_level = G_LOG_LEVEL_ERROR;
static FILE * old_stdout = NULL;


int client_log_get_level_for_domain(const gchar * domain) {
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


static GLogWriterOutput log_to_file(GLogLevelFlags log_level, const GLogField * fields,
                                    gsize n_fields, gpointer user_data) {
    gsize i;
    const gchar * log_domain = NULL, * message = NULL, * log_level_str;
    gboolean fatal = log_level & G_LOG_FLAG_FATAL;
    log_level &= G_LOG_LEVEL_MASK;

    GLogLevelFlags max_level = default_level;
    if (level_for_domains != NULL) {
        for (i = 0; i < n_fields; ++i) {
            if (g_str_equal(fields[i].key, "GLIB_DOMAIN")) {
                log_domain = (const gchar *)fields[i].value;
                max_level = client_log_get_level_for_domain(log_domain);
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

#ifdef ANDROID
    android_LogPriority android_log_level = ANDROID_LOG_UNKNOWN;
    switch (log_level) {
        case G_LOG_LEVEL_ERROR: android_log_level = ANDROID_LOG_FATAL; break;
        case G_LOG_LEVEL_CRITICAL: android_log_level = ANDROID_LOG_ERROR; break;
        case G_LOG_LEVEL_WARNING: android_log_level = ANDROID_LOG_WARN; break;
        case G_LOG_LEVEL_MESSAGE: android_log_level = ANDROID_LOG_INFO; break;
        case G_LOG_LEVEL_INFO: android_log_level = ANDROID_LOG_INFO; break;
        default: android_log_level = ANDROID_LOG_DEBUG; break;
    }
    __android_log_print(android_log_level, log_domain, "%s", message);
#else
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree gchar * now_str = g_date_time_format(now, "%Y/%m/%d %H:%M:%S");
    fprintf(stderr, "%s.%03d: %s-%s: %s\n", now_str, g_date_time_get_microsecond(now)/1000,
            log_domain, log_level_str, message);
#endif

    if (fatal || log_level <= fatal_level) G_BREAKPOINT();

    return G_LOG_WRITER_HANDLED;
}

static gboolean map_level(int level_num, GLogLevelFlags * level) {
    GLogLevelFlags map[] = {
        G_LOG_LEVEL_ERROR, G_LOG_LEVEL_CRITICAL, G_LOG_LEVEL_WARNING,
        G_LOG_LEVEL_MESSAGE, G_LOG_LEVEL_INFO, G_LOG_LEVEL_DEBUG
    };

    if (level_num >= 0 && level_num <= 5) {
        *level = map[level_num];
        return TRUE;
    }
    return FALSE;
}

void client_log_setup() {
#ifndef ANDROID
    old_stdout = fdopen(dup(1), "a");
    if (!g_getenv("FLEXVDI_LOG_STDERR")) {
        g_autofree gchar * file_path = g_strdup(g_getenv("FLEXVDI_LOG_FILE"));
        if (!file_path) {
            g_autofree gchar * log_dir = g_build_filename(g_get_user_data_dir(), "flexvdi-client", NULL);
            g_mkdir_with_parents(log_dir, 0700);
            file_path = g_build_filename(log_dir, "flexvdi-client.log", NULL);
        }
        freopen(file_path, "a", stderr);
        freopen(file_path, "a", stdout);
    }
    setvbuf(stderr, NULL, _IONBF, 2);
#endif

    g_log_set_writer_func(log_to_file, NULL, NULL);

    const gchar * fatal_str = g_getenv("FLEXVDI_FATAL_LEVEL");
    if (fatal_str) {
        map_level(strtol(fatal_str, NULL, 10), &fatal_level);
        g_log_set_always_fatal(fatal_level);
    }
}


void print_to_stdout(const gchar * string) {
    fprintf(old_stdout, "%s", string);
}


void client_log_set_log_levels(const gchar * verbose_str) {
    g_list_free_full(level_for_domains, g_free);
    level_for_domains = NULL;
    default_level = G_LOG_LEVEL_MESSAGE;

    gchar ** levels = g_strsplit(verbose_str, ",", 0), ** level;
    for (level = levels; *level; ++level) {
        if (**level == '\0') continue;

        gchar ** terms = g_strsplit(*level, ":", 0);
        if (*(terms + 1) == NULL) {
            map_level(strtol(*terms, NULL, 10), &default_level);
        } else {
            GLogLevelFlags glevel;
            if (map_level(strtol(*(terms + 1), NULL, 10), &glevel)) {
                if (g_str_equal(*terms, "all")) {
                    default_level = glevel;
                } else {
                    update_level_for_domain(*terms, glevel);
                }
            }
        }

        g_strfreev(terms);
    }

    g_strfreev(levels);

    if (client_log_get_level_for_domain("GSpice") == G_LOG_LEVEL_DEBUG)
        g_setenv("SPICE_DEBUG", "1", TRUE);
    else
        g_unsetenv("SPICE_DEBUG");
}
