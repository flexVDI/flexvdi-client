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
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include "printclient.h"
#include "printclient-priv.h"
#include "flexvdi-port.h"

struct _PrintJobManager {
    GObject parent;
    GHashTable * print_jobs;
};

enum {
    PRINT_JOB_MANAGER_PDF = 0,
    PRINT_JOB_MANAGER_LAST_SIGNAL
};

static guint signals[PRINT_JOB_MANAGER_LAST_SIGNAL];

G_DEFINE_TYPE(PrintJobManager, print_job_manager, G_TYPE_OBJECT);


static void print_job_manager_finalize(GObject * obj);

static void print_job_manager_class_init(PrintJobManagerClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->finalize = print_job_manager_finalize;

    // Emited when a job is not printed and the file remains
    signals[PRINT_JOB_MANAGER_PDF] =
        g_signal_new("pdf",
                     PRINT_JOB_MANAGER_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_POINTER);
}


static gboolean remove_temp_files(gpointer user_data);

static void print_job_manager_init(PrintJobManager * pjb) {
    pjb->print_jobs = g_hash_table_new_full(g_direct_hash, NULL, NULL, g_free);
    g_timeout_add_seconds(300, remove_temp_files, NULL);
}


static void print_job_manager_finalize(GObject * obj) {
    PrintJobManager * pjb = PRINT_JOB_MANAGER(obj);
    g_hash_table_unref(pjb->print_jobs);
    G_OBJECT_CLASS(print_job_manager_parent_class)->finalize(obj);
}


PrintJobManager * print_job_manager_new() {
    return g_object_new(PRINT_JOB_MANAGER_TYPE, NULL);
}


static gboolean remove_temp_files(gpointer user_data) {
    const gchar * tmp_dir_name = g_get_tmp_dir();
    GDir * tmp_dir = g_dir_open(tmp_dir_name, 0, NULL);
    if (tmp_dir) {
        gchar file[1024];
        const gchar * basename;
        GStatBuf file_stat;
        time_t now = time(NULL);
        GRegex * regex = g_regex_new("fpj......\\.pdf$", 0, 0, NULL);
        while ((basename = g_dir_read_name(tmp_dir))) {
            snprintf(file, 1024, "%s/%s", tmp_dir_name, basename);
            g_stat(file, &file_stat);
            time_t elapsed = now - file_stat.st_mtime;
            // Remove job files that were last modified at least 5 minutes ago
            if (g_regex_match(regex, file, 0, NULL) && elapsed > 300) {
                g_unlink(file);
            }
        }
        g_dir_close(tmp_dir);
        g_regex_unref(regex);
    }
    return TRUE;
}


static void handle_print_job(PrintJobManager * pjb, FlexVDIPrintJobMsg * msg) {
    PrintJob * job = g_malloc(sizeof(PrintJob));
    job->file_handle = g_file_open_tmp("fpjXXXXXX.pdf", &job->name, NULL);
    job->options = g_strndup(msg->options, msg->optionsLength);
    g_debug("Job %s, Options: %.*s", job->name, msg->optionsLength, msg->options);
    g_hash_table_insert(pjb->print_jobs, GINT_TO_POINTER(msg->id), job);
}


static void handle_print_job_data(PrintJobManager * pjb, FlexVDIPrintJobDataMsg * msg) {
    PrintJob * job = g_hash_table_lookup(pjb->print_jobs, GINT_TO_POINTER(msg->id));
    if (job) {
        if (!msg->dataLength) {
            close(job->file_handle);
            if (!print_job(job))
                g_signal_emit(pjb, signals[PRINT_JOB_MANAGER_PDF], 0, job->name);
            g_free(job->name);
            g_hash_table_remove(pjb->print_jobs, GINT_TO_POINTER(msg->id));
        } else {
            write(job->file_handle, msg->data, msg->dataLength);
        }
    } else {
        g_info("Job %d not found", msg->id);
    }
}


gboolean print_job_manager_handle_message(
        PrintJobManager * pjb, uint32_t type, gpointer data) {
    switch (type) {
    case FLEXVDI_PRINTJOB:
        handle_print_job(pjb, (FlexVDIPrintJobMsg *)data);
        break;
    case FLEXVDI_PRINTJOBDATA:
        handle_print_job_data(pjb, (FlexVDIPrintJobDataMsg *)data);
        break;
    default:
        return FALSE;
    }
    g_free(data);
    return TRUE;
}


char * get_job_options(char * options, const char * op_name) {
    gunichar equal = g_utf8_get_char("="),
             space = g_utf8_get_char(" "),
             quotes = g_utf8_get_char("\"");
    int op_len = strlen(op_name);
    gunichar next_char;
    char * op_pos = options, * op_end = options + strlen(options);
    do {
        op_pos = strstr(op_pos, op_name);
        if (!op_pos) return NULL;
        op_pos += op_len;
        next_char = g_utf8_get_char(op_pos);
    } while ((op_pos > options + op_len && g_utf8_get_char(op_pos - op_len - 1) != space) ||
             (op_pos < op_end && next_char != equal && next_char != space));
    int value_len = 0;
    if (next_char == equal) {
        op_pos = g_utf8_next_char(op_pos);
        gunichar delimiter = space;
        if (g_utf8_get_char(op_pos) == quotes) {
            op_pos = g_utf8_next_char(op_pos);
            delimiter = quotes;
        }
        char * end = g_utf8_strchr(op_pos, -1, delimiter);
        if (!end) end = op_end;
        value_len = end - op_pos;
    }
    char * result = g_malloc(value_len + 1);
    memcpy(result, op_pos, value_len);
    result[value_len] = '\0';
    return result;
}


int flexvdi_share_printer(FlexvdiPort * port, const char * printer) {
    if (!flexvdi_port_is_agent_connected(port)) {
        g_warning("The flexVDI guest agent is not connected");
        return FALSE;
    }
    g_debug("Sharing printer %s", printer);

    int result = FALSE;
    size_t buf_size, name_len, ppd_len;
    GStatBuf stat_buf;
    g_autofree gchar * ppd_name = get_ppd_file(printer);
    if (ppd_name == NULL) return FALSE;
    name_len = strlen(printer);
    if (!g_stat(ppd_name, &stat_buf)) {
        ppd_len = stat_buf.st_size;
        buf_size = sizeof(FlexVDISharePrinterMsg) + name_len + 1 + ppd_len;
        uint8_t * buf = flexvdi_port_get_msg_buffer(buf_size);
        if (buf) {
            FILE * ppd = g_fopen(ppd_name, "r");
            if (ppd) {
                FlexVDISharePrinterMsg * msg = (FlexVDISharePrinterMsg *)buf;
                msg->printerNameLength = name_len;
                msg->ppdLength = ppd_len;
                strncpy(msg->data, printer, name_len + 1);
                fread(&msg->data[name_len + 1], 1, ppd_len, ppd);
                flexvdi_port_send_msg(port, FLEXVDI_SHAREPRINTER, buf);
                result = TRUE;
            } else g_warning("Failed to open PPD file %s", ppd_name);
            fclose(ppd);
        } else g_warning("Unable to reserve memory for printer message");
    }
    g_unlink(ppd_name);
    return result;
}


int flexvdi_unshare_printer(FlexvdiPort * port, const char * printer) {
    if (!flexvdi_port_is_agent_connected(port)) {
        g_warning("The flexVDI guest agent is not connected");
        return FALSE;
    }
    g_debug("Unsharing printer %s", printer);
    size_t name_len = strlen(printer);
    size_t buf_size = sizeof(FlexVDIUnsharePrinterMsg) + name_len + 1;
    uint8_t * buf = flexvdi_port_get_msg_buffer(buf_size);
    if (buf) {
        FlexVDIUnsharePrinterMsg * msg = (FlexVDIUnsharePrinterMsg *)buf;
        msg->printerNameLength = name_len;
        strncpy(msg->printerName, printer, name_len + 1);
        flexvdi_port_send_msg(port, FLEXVDI_UNSHAREPRINTER, buf);
        return TRUE;
    } else {
        g_warning("Unable to reserve memory for printer message");
        return FALSE;
    }
}
