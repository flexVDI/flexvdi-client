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
#include "flexvdi-spice.h"

static GHashTable * printJobs;


static gboolean removeTempFiles(gpointer user_data) {
    const gchar * tmpDirName = g_get_tmp_dir();
    GDir * tmpDir = g_dir_open(tmpDirName, 0, NULL);
    if (tmpDir) {
        gchar file[1024];
        const gchar * basename;
        GStatBuf fileStat;
        time_t now = time(NULL);
        GRegex * regex = g_regex_new("fpj......\\.pdf$", 0, 0, NULL);
        while ((basename = g_dir_read_name(tmpDir))) {
            snprintf(file, 1024, "%s/%s", tmpDirName, basename);
            g_stat(file, &fileStat);
            time_t elapsed = now - fileStat.st_mtime;
            // Remove job files that were last modified at least 5 minutes ago
            if (g_regex_match(regex, file, 0, NULL) && elapsed > 300) {
                g_unlink(file);
            }
        }
        g_dir_close(tmpDir);
        g_regex_unref(regex);
    }
    return TRUE;
}


void handlePrintJob(FlexVDIPrintJobMsg * msg) {
    PrintJob * job = g_malloc(sizeof(PrintJob));
    job->fileHandle = g_file_open_tmp("fpjXXXXXX.pdf", &job->name, NULL);
    job->options = g_strndup(msg->options, msg->optionsLength);
    flexvdiLog(L_DEBUG, "Job %s, Options: %.*s", job->name, msg->optionsLength, msg->options);
    g_hash_table_insert(printJobs, GINT_TO_POINTER(msg->id), job);
}


void handlePrintJobData(FlexVDIPrintJobDataMsg * msg) {
    PrintJob * job = g_hash_table_lookup(printJobs, GINT_TO_POINTER(msg->id));
    if (job) {
        if (!msg->dataLength) {
            close(job->fileHandle);
            printJob(job);
            g_free(job->name);
            g_hash_table_remove(printJobs, GINT_TO_POINTER(msg->id));
        } else {
            write(job->fileHandle, msg->data, msg->dataLength);
        }
    } else {
        flexvdiLog(L_INFO, "Job %d not found", msg->id);
    }
}


char * getJobOption(char * options, const char * opName) {
    gunichar equalSign = g_utf8_get_char("="),
             space = g_utf8_get_char(" "),
             quotes = g_utf8_get_char("\"");
    int opLen = strlen(opName);
    gunichar nextChar;
    char * opPos = options, * opEnd = options + strlen(options);
    do {
        opPos = strstr(opPos, opName);
        if (!opPos) return NULL;
        opPos += opLen;
        nextChar = g_utf8_get_char(opPos);
    } while ((opPos > options + opLen && g_utf8_get_char(opPos - opLen - 1) != space) ||
             (opPos < opEnd && nextChar != equalSign && nextChar != space));
    int valueLen = 0;
    if (nextChar == equalSign) {
        opPos = g_utf8_next_char(opPos);
        gunichar delimiter = space;
        if (g_utf8_get_char(opPos) == quotes) {
            opPos = g_utf8_next_char(opPos);
            delimiter = quotes;
        }
        char * end = g_utf8_strchr(opPos, -1, delimiter);
        if (!end) end = opEnd;
        valueLen = end - opPos;
    }
    char * result = g_malloc(valueLen + 1);
    memcpy(result, opPos, valueLen);
    result[valueLen] = '\0';
    return result;
}


void initPrintClient() {
    printJobs = g_hash_table_new_full(g_direct_hash, NULL, NULL, g_free);
    g_timeout_add_seconds(300, removeTempFiles, NULL);
}


void flexvdiSpiceSharePrinter(const char * printer) {
    size_t bufSize, nameLength, ppdLength;
    GStatBuf statbuf;
    char * ppdName = getPPDFile(printer);
    nameLength = strnlen(printer, 1024);
    if (!g_stat(ppdName, &statbuf)) {
        ppdLength = statbuf.st_size;
        bufSize = sizeof(FlexVDISharePrinterMsg) + nameLength + 1 + ppdLength;
        uint8_t * buf = getMsgBuffer(bufSize);
        if (buf) {
            FILE * ppd = g_fopen(ppdName, "r");
            if (ppd) {
                FlexVDISharePrinterMsg * msg = (FlexVDISharePrinterMsg *)buf;
                msg->printerNameLength = nameLength;
                msg->ppdLength = ppdLength;
                strncpy(msg->data, printer, nameLength + 1);
                fread(&msg->data[nameLength + 1], 1, ppdLength, ppd);
                sendMessage(FLEXVDI_SHAREPRINTER, buf);
            }
            fclose(ppd);
        } else g_warning("Unable to reserve memory for printer message");
    }
    g_unlink(ppdName);
    g_free(ppdName);
}


void flexvdiSpiceUnsharePrinter(const char * printer) {
    size_t nameLength = strnlen(printer, 1024);
    size_t bufSize = sizeof(FlexVDIUnsharePrinterMsg) + nameLength + 1;
    uint8_t * buf = getMsgBuffer(bufSize);
    if (buf) {
        FlexVDIUnsharePrinterMsg * msg = (FlexVDIUnsharePrinterMsg *)buf;
        msg->printerNameLength = nameLength;
        strncpy(msg->printerName, printer, nameLength + 1);
        sendMessage(FLEXVDI_UNSHAREPRINTER, buf);
    } g_warning("Unable to reserve memory for printer message");
}
