/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include "printclient.h"
#include "flexvdi-port.h"

static GHashTable * print_jobs;


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


void handle_print_job(FlexVDIPrintJobMsg * msg) {
    PrintJob * job = g_malloc(sizeof(PrintJob));
    job->file_handle = g_file_open_tmp("fpjXXXXXX.pdf", &job->name, NULL);
    job->options = g_strndup(msg->options, msg->optionsLength);
    g_debug("Job %s, Options: %.*s", job->name, msg->optionsLength, msg->options);
    g_hash_table_insert(print_jobs, GINT_TO_POINTER(msg->id), job);
}


void handle_print_job_data(FlexVDIPrintJobDataMsg * msg) {
    PrintJob * job = g_hash_table_lookup(print_jobs, GINT_TO_POINTER(msg->id));
    if (job) {
        if (!msg->dataLength) {
            close(job->file_handle);
            print_job(job);
            g_free(job->name);
            g_hash_table_remove(print_jobs, GINT_TO_POINTER(msg->id));
        } else {
            write(job->file_handle, msg->data, msg->dataLength);
        }
    } else {
        g_info("Job %d not found", msg->id);
    }
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


void init_print_client() {
    print_jobs = g_hash_table_new_full(g_direct_hash, NULL, NULL, g_free);
    g_timeout_add_seconds(300, remove_temp_files, NULL);
}


int flexvdi_share_printer(const char * printer) {
    if (!flexvdi_is_agent_connected()) {
        g_warning("The flexVDI guest agent is not connected");
        return FALSE;
    }
    g_debug("Sharing printer %s", printer);

    int result = FALSE;
    size_t buf_size, name_len, ppd_len;
    GStatBuf stat_buf;
    g_autofree gchar * ppd_name = get_ppd_file(printer);
    name_len = strnlen(printer, 1024);
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
                flexvdi_port_send_msg(FLEXVDI_SHAREPRINTER, buf);
                result = TRUE;
            } else g_warning("Failed to open PPD file %s", ppd_name);
            fclose(ppd);
        } else g_warning("Unable to reserve memory for printer message");
    }
    g_unlink(ppd_name);
    return result;
}


int flexvdi_unshare_printer(const char * printer) {
    if (!flexvdi_is_agent_connected()) {
        g_warning("The flexVDI guest agent is not connected");
        return FALSE;
    }
    g_debug("Unsharing printer %s", printer);
    size_t name_len = strnlen(printer, 1024);
    size_t buf_size = sizeof(FlexVDIUnsharePrinterMsg) + name_len + 1;
    uint8_t * buf = flexvdi_port_get_msg_buffer(buf_size);
    if (buf) {
        FlexVDIUnsharePrinterMsg * msg = (FlexVDIUnsharePrinterMsg *)buf;
        msg->printerNameLength = name_len;
        strncpy(msg->printerName, printer, name_len + 1);
        flexvdi_port_send_msg(FLEXVDI_UNSHAREPRINTER, buf);
        return TRUE;
    } else {
        g_warning("Unable to reserve memory for printer message");
        return FALSE;
    }
}
