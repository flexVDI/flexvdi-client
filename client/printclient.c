/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "printclient.h"

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
        while (basename = g_dir_read_name(tmpDir)) {
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
    fprintf(stderr, "Job %s, Options: %.*s\n", job->name, msg->optionsLength, msg->options);
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
        printf("Job %d not found\n", msg->id);
    }
}


char * getJobOption(char * options, const char * opName) {
    int opLen = strlen(opName);
    char * opPos = strstr(options, opName);
    if (!opPos) return NULL;
    opPos += opLen;
    int valueLen = 0;
    if (*opPos == '=') {
        ++opPos;
        char delimiter = ' ';
        if (*opPos == '"') {
            ++opPos;
            delimiter = '"';
        }
        char * end = strchr(opPos, delimiter);
        if (!end) return NULL;
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
